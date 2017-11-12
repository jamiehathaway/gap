/**
 * @file capture_example.hh
 * 
 * @brief Example application using object spawner and camera plugins
 * 
 * This example spawns randomly generated objects and captures images from 
 * a camera in a given position.
 * 
 * It requires that
 * - gazebo-utils/media folder is adequatly populated with different materials
 * - gazebo-utils/model folder has the custom_camera.sdf, custom_sun.sdf and custom_ground.sdf models
 * - output_dir exists 
 *
 * @param[in]  _argc  The number of command-line arguments
 * @param      _argv  The argv The value of the command-line arguments
 *
 * @return     0
 */

#include "capture_example.hh"
#include <Eigen/Dense>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

double dRand(double fMin, double fMax)
{
    /* Initialize random device */
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist;

    double f = (double) dist(mt) / RAND_MAX;
    return fMin + f * (fMax - fMin);
}

/** Protect access to object_count */
std::mutex object_count_mutex;
int object_count{0};
/** Protect access to camera success */
std::mutex camera_success_mutex;
int camera_success{0};


BoundingBox3d bbs_3d;
BoundingBox2d points_2d;

std::vector<Object> objects;
std::vector<std::string> object_classes;
std::vector<int> cells_array;

int box_counter=0;
int cylinder_counter=0;
int sphere_counter=0;


const unsigned int x_cells = 10;
const unsigned int y_cells = 10;
double grid_cell_size = 0.5;

int min_objects = 5;
int max_objects = 10;
double tx=2.5;
double ty=2.5;
double tz=3.5;

int main(int argc, char **argv)
{

    /* TODO - Process options */
    if (argc < 4)
    {
        std::cout << "invalid number of arguments"<< std::endl;
        exit(-1);
    }

    std::string media_dir = std::string(argv[1]);
    unsigned int scenes = atoi(argv[2]);
    std::string train_dir = std::string(argv[3]);
	
    /* Create folder for storing training data */
    boost::filesystem::path traindir(train_dir);
    if(boost::filesystem::create_directory(traindir)) {
	std::cout << "Successfuly created train directory:" << traindir << std::endl;
    }


    std::string materials_dir   = media_dir + "/materials";
    std::string scripts_dir     = media_dir + "/materials/scripts";

    /* Initialize random device */
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist;

    /* Load gazebo as a client */
    #if GAZEBO_MAJOR_VERSION < 6
    gazebo::setupClient(argc, argv);
    #else
    gazebo::client::setup(argc, argv);
    #endif

    /* Create the communication node */
    gazebo::transport::NodePtr node(new gazebo::transport::Node());
    node->Init();

    /* Publish to the object spawner request topic */
    gazebo::transport::PublisherPtr pub_world =
        node->Advertise<world_utils::msgs::WorldUtilsRequest>(WORLD_UTILS_TOPIC);

    /* Subscribe to the object spawner reply topic and link callback function */
    gazebo::transport::SubscriberPtr sub_world =
        node->Subscribe(WORLD_UTILS_RESPONSE_TOPIC, onWorldUtilsResponse);

    /* Publish to the camera topic */
    gazebo::transport::PublisherPtr pub_camera =
        node->Advertise<camera_utils::msgs::CameraUtilsRequest>(CAMERA_UTILS_TOPIC);

     /* Subscribe to the camera utils reply topic and link callback function */
    gazebo::transport::SubscriberPtr sub_camera =
        node->Subscribe(CAMERA_UTILS_RESPONSE_TOPIC, onCameraUtilsResponse);

    /* Wait for a subscriber to connect */
    pub_world->WaitForConnection();

    /* Create a vector with the name of every texture in the textures dir */
    std::vector<std::string> textures;
    for (auto &p : fs::directory_iterator(scripts_dir)){
        std::string aux(fs::basename(p));
        textures.push_back(aux.c_str());
    }

    /* Auxiliary variables */
    //ignition::math::Quaternion<double> camera_orientation(0, M_PI/2.0, 0);
    ignition::math::Quaternion<double> camera_orientation(0, M_PI / 2.0, 0);


    /* Ensure no objects are spawned on the server */
    clearWorld(pub_world);
    //clearWorld(pub_world,"plugin");
    while (waitForSpawner(0)){
        usleep(1000);
        queryModelCount(pub_world);
    }
    sleep(2);
    spawnModelFromFile( pub_world, "models/custom_sun.sdf", true, false, false, textures);
    spawnModelFromFile(pub_world, "models/custom_camera.sdf", false, true, false, textures,  tx, ty, tz, camera_orientation);
    pub_camera->WaitForConnection();
    
    while (waitForSpawner(1)){
      usleep(1000);
      queryModelCount(pub_world);
    }

    /* Create cell grid */

    for (int i = 0;  i < x_cells * y_cells; ++i){
      cells_array.push_back(i);
    }
    
    /* Disable physics */
    changePhysics(pub_world, false);

    /* Main loop */
    for (int i = 0; i < scenes; i++){
    	    while (pub_world->GetOutgoingCount()>0){
	      usleep(1000);
	    }
        /* Random object number */
        int num_objects = (dist(mt) % max_objects) + min_objects;

        /* DEBUG */
        std::cout << "Scene " << i << " - Number of objects:" << num_objects << std::endl;
        

        /* Spawn ground */
        spawnModelFromFile(pub_world, "models/custom_ground.sdf", false, false, true, textures);

        while (waitForSpawner(2)){
            usleep(1000);
            queryModelCount(pub_world);
        }

	while (pub_world->GetOutgoingCount()>0){
		usleep(1000);
	}

        /* Spawn random objects */

        // TODO - Organise random generator
        std::mt19937 g(rd());
        std::shuffle(cells_array.begin(), cells_array.end(), g);
 
        //std::copy(cells_array.begin(), cells_array.end(), std::ostream_iterator<int>(std::cout, " "));
	objects.clear();



		/*while (waitForSpawner(j+2)){
		usleep(1000);
		queryModelCount(pub_world);
		}*/
	spawnRandomObject(pub_world, textures, grid_cell_size, num_objects, objects);
        

	/*while (pub_world->GetOutgoingCount()>0){
		std::cout << pub_world->GetOutgoingCount() << std::endl;
		usleep(100000);
	}*/
        while (waitForSpawner(num_objects + 2)){
            usleep(1000);

            queryModelCount(pub_world);
        }
	    while (pub_world->GetOutgoingCount()>0){
	      usleep(1000);
	    }


        /* Disable physics */
        //changePhysics(pub_world, false);
        /* Still needed! */
        sleep(1.0);
        /* Capture the scene and save it to a file */
        captureScene(pub_camera, i);
        /*while (waitForCamera()){

            usleep(100000);
        }*/
	while (pub_camera->GetOutgoingCount()>0){
		usleep(100000);
	}


        sleep(0.5);
        /* Disable physics */
        //changePhysics(pub_world, true);

        /* get 3d bounding boxes */
        bbs_3d.clear();
	queryModelBoundingBox(pub_world, objects);


	while (pub_world->GetOutgoingCount()>0){
		usleep(10000);
	}

	if(bbs_3d.size()!=num_objects)
	{
		while (bbs_3d.size()!=num_objects){
			usleep(10000);
		}
	}

	/* Query 2D image points given 3d bounding box (8 3d points) */
	// TODO - Apparently responses are not synchronous (points belonging to different objects become mixed): possible solution: query2DcameraPoint could send an object identifier
	points_2d.clear();

	query2DcameraPoint(pub_camera,objects);

	while (pub_camera->GetOutgoingCount()>0){
		usleep(10000);
	}
        while (points_2d.size()<num_objects){
            usleep(1000);
        }

	/* Get bounding boxes */
	std::vector<cv::Rect> boundRect( num_objects );

	for(int j=0;j<num_objects;++j)
	{
		std::pair <BoundingBox2d::iterator, BoundingBox2d::iterator> ret;
		ret = points_2d.equal_range(objects[j].name);
		std::vector<cv::Point> contours_poly( 8 );
		int p=0;

		for (std::multimap<std::string,ignition::math::Vector2d>::iterator it=ret.first; it!=ret.second; ++it)
		{	
			contours_poly[p++]=cv::Point(it->second.X(),it->second.Y());
		}

       		boundRect[j]=cv::boundingRect( cv::Mat(contours_poly) );

		objects[j].bounding_box=boundRect[j];
	}


	/* Save annotations */
	storeAnnotations(objects, train_dir, std::to_string(i)+".xml");

	// TODO - SAVE TO FILE (SEE OBJECT DETECTION DATASETS. Eg. pascal voc)

	/*cv::Mat image;
	image = cv::imread("/tmp/camera_utils_output/"+std::to_string(i)+".png", CV_LOAD_IMAGE_COLOR);   // Read the file

	if(! image.data )                              // Check for invalid input
	{
           std::cout <<  "Could not open or find the image" << std::endl ;
           return -1;
	}

	for(int j=0;j<num_objects;++j)
	{
		std::pair <BoundingBox2d::iterator, BoundingBox2d::iterator> ret;
		ret = points_2d.equal_range(objects[j].name);

		for (std::multimap<std::string,ignition::math::Vector2d>::iterator it=ret.first; it!=ret.second; ++it)
		{

			cv::circle(image, cv::Point(it->second.X(),it->second.Y()), 5, cv::Scalar(255,0,1));
		}
       		rectangle( image, boundRect[j].tl(), boundRect[j].br(), cv::Scalar(255,0,1), 2, 8, 0 );
	}
	cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );// Create a window for display.
	cv::imshow( "Display window", image );                   // Show our image inside it.
	cv::waitKey(1000);                                          // Wait for a keystroke in the window
		*/
        /* Clear the scene */
        /*clearWorld(pub_world);
        //clearWorld(pub_world,"plugin");
        while (waitForSpawner(0)){
           usleep(1000);
           queryModelCount(pub_world);
        }*/

	for(int j=0; j < num_objects;++j)
	{
        	clearWorld(pub_world, objects[j].name);
	}

	clearWorld(pub_world,"plugin_ground_plane");
	//clearWorld(pub_world,"plugin");
        // TODO - Move camera and light source
        while (waitForSpawner(1)){
            usleep(10000);
            queryModelCount(pub_world);
        }//*/

        //sleep(0.1);
    }

    /* Shut down */
    #if GAZEBO_MAJOR_VERSION < 6
    gazebo::shutdown();
    #else
    gazebo::client::shutdown();
    #endif

    return 0;
}

/* Spawn objects */

void spawnModelFromFile(
    gazebo::transport::PublisherPtr pub,
    const std::string model_path,
    const bool is_light,
    const bool use_custom_pose,
    const bool use_custom_textures,
    std::vector<std::string> textures,
    const double & px, 
    const double & py,
    const double & pz,
    const ignition::math::Quaternion<double> & orientation){

    /* Read model sdf string from file */
    std::ifstream infile {model_path};
    std::string model_sdf { std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>() };
    
    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(SPAWN);
    world_utils::msgs::Object* object = msg.add_object();
    if (is_light){
        object->set_model_type(CUSTOM_LIGHT);
    } else {
        object->set_model_type(CUSTOM);
    }
    object->set_sdf(model_sdf);

    if (use_custom_pose){
        gazebo::msgs::Vector3d *pos = new gazebo::msgs::Vector3d();
        gazebo::msgs::Quaternion *ori = new gazebo::msgs::Quaternion(gazebo::msgs::Convert(orientation));
        gazebo::msgs::Pose *pose = new gazebo::msgs::Pose();
        pos->set_x(px);
        pos->set_y(py);
        pos->set_z(pz);
        pose->set_allocated_position(pos);
        pose->set_allocated_orientation(ori);
        object->set_allocated_pose(pose);
    }

    if (use_custom_textures){
    /* Initialize random device */
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist;
        int j=dist(mt) % textures.size();

        std::string texture = textures.at(j);
        std::stringstream texture_uri;
        std::stringstream texture_name;

        texture_uri << "file://materials/scripts/" << texture << ".material"
        << "</uri><uri>file://materials/textures/";
        texture_name << "Plugin/" << texture;

        object->set_texture_uri(texture_uri.str());
        object->set_texture_name(texture_name.str());
    }
    pub->Publish(msg);
}

void spawnRandomObject(
    gazebo::transport::PublisherPtr pub,
    std::vector<std::string> textures,
    double & grid_cell_size,
    int & num_objects,
    std::vector<Object> & objects){



    /* Initialize random device */
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist;

    world_utils::msgs::WorldUtilsRequest msg;
    
    msg.set_type(SPAWN);


    for(int i=0; i<num_objects;++i)
    {
            unsigned int x_cell = floor(cells_array[i] / x_cells);
            unsigned int y_cell = floor(cells_array[i] - x_cell * x_cells);
	    std::string object_name;
	    int object_type=dist(mt) % 3;

	    world_utils::msgs::Object* object = msg.add_object();
	    if (object_type == CYLINDER_ID){
		object->set_model_type(CYLINDER);
		object_name="plugin_cylinder_"+std::to_string(cylinder_counter++);
		object->set_name(object_name);
	    }
	    else if(object_type == BOX_ID){
		object->set_model_type(BOX);
		object_name="plugin_box_"+std::to_string(box_counter++);
		object->set_name(object_name);
	    }
	    else if(object_type == SPHERE_ID){
		object->set_model_type(SPHERE);
		object_name="plugin_sphere_"+std::to_string(sphere_counter++);
		object->set_name(object_name);
	    }

	    /* External optional fields have to be allocated */
	    gazebo::msgs::Vector3d *pos = new gazebo::msgs::Vector3d();
	    gazebo::msgs::Quaternion *ori = new gazebo::msgs::Quaternion();
	    gazebo::msgs::Pose *pose = new gazebo::msgs::Pose();
	    gazebo::msgs::Vector3d *size = new gazebo::msgs::Vector3d();

	    /*
	    ori->set_x(0.0);
	    ori->set_y(0.0);
	    ori->set_z(0.0);
	    ori->set_w(1.0);
	    */
	    
	    /* Mass */
	    object->set_mass(dist(mt) % 5 + 1.0);
	    /* Sphere/cylinder radius */
	    double radius=dRand(0.1, grid_cell_size * 0.5);

	    object->set_radius(radius);

	    /* Box size */ 
	    double x_length = dRand(0.1, grid_cell_size);
	    double y_length = dRand(0.1, grid_cell_size);    
	    double z_length = dRand(0.1, grid_cell_size);

	    size->set_x(x_length);
	    size->set_y(y_length);
	    size->set_z(z_length);
	    
	    /* Cylinder/Sphere length */
	    object->set_length(z_length);

	    /* Pose */
	    ignition::math::Quaternion<double> object_orientation;

	    if (dRand(0.0, 1.0) < 0.5){

		// Horizontal
		double yaw = dRand(0.0,M_PI);
		object_orientation=ignition::math::Quaternion<double> (0.0, M_PI*0.5, yaw);

		if(object_type == CYLINDER_ID || object_type == SPHERE_ID ) {
		   pos->set_z(radius);
	       }
	       else if(object_type == BOX_ID)
		   pos->set_z(x_length*0.5); // height is radius
	    } else {

	       double roll = dRand(0.0,M_PI);
	       double pitch = dRand(0.0,M_PI);

	       // Vertical
	       object_orientation=ignition::math::Quaternion<double> (0.0, 0.0, 0.0);
	       pos->set_z(z_length*0.5);
	       if(object_type == CYLINDER_ID || object_type == SPHERE_ID) {
		   pos->set_z(radius);
	       }
	    }

	    pos->set_x(x_cell * grid_cell_size + 0.5 * grid_cell_size);
	    pos->set_y(y_cell * grid_cell_size + 0.5 * grid_cell_size);

	    ori=new gazebo::msgs::Quaternion(gazebo::msgs::Convert(object_orientation));

	    /* Material script */
	    int j = dist(mt) % textures.size();

	    std::string texture = textures.at(j);
	    std::stringstream texture_uri;
	    std::stringstream texture_name;

	    texture_uri << "file://materials/scripts/" << texture << ".material"
	    << "</uri><uri>file://materials/textures/";
	    texture_name << "Plugin/" << texture;

	    object->set_texture_uri(texture_uri.str());
	    object->set_texture_name(texture_name.str());

	    /* Associate dynamic fields */
	    pose->set_allocated_position(pos);
	    pose->set_allocated_orientation(ori);
	    object->set_allocated_pose(pose);
	    object->set_allocated_box_size(size);

   	 objects.push_back( Object(object_name,object_type));
    }
    /* Send the message */
    pub->Publish(msg);

}

void clearWorld(gazebo::transport::PublisherPtr pub, std::string name){
    std::cout <<"limpar" << std::endl;
    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(REMOVE);
    // Only remove models that match the string (exclude custom_camera)

    if(!name.empty())
    {
        world_utils::msgs::Object* object = msg.add_object();
 	object->set_name(name);
    }
    pub->Publish(msg);
}

void changePhysics(gazebo::transport::PublisherPtr pub, bool enable){
    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(PHYSICS);
    msg.set_state(enable);
    pub->Publish(msg);
}

void pauseWorld(gazebo::transport::PublisherPtr pub, bool enable){
    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(PAUSE);
    msg.set_state(enable);
    pub->Publish(msg);
}


void captureScene(gazebo::transport::PublisherPtr pub, int j){
    camera_utils::msgs::CameraUtilsRequest msg;
    msg.set_type(CAPTURE);
    msg.set_file_name(std::to_string(j));
    pub->Publish(msg,false);
}

/* Handle object count */

bool waitForSpawner(int desired_objects){
    std::lock_guard<std::mutex> lock(object_count_mutex);

    if (desired_objects == object_count)
        return false;
    return true;
}

void queryModelCount(gazebo::transport::PublisherPtr pub){
    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(STATUS);
    pub->Publish(msg,false);
}

void queryModelBoundingBox(
    gazebo::transport::PublisherPtr pub,
    const std::vector<Object> & objects){
    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(STATUS);
    for(int i(0);i<objects.size();++i)
    {
   		world_utils::msgs::BoundingBox* bounding_box = msg.add_bounding_box();
                bounding_box->set_name(objects[i].name);
    }

    pub->Publish(msg,false);
}

void query2DcameraPoint(
    gazebo::transport::PublisherPtr pub,
    const std::vector<Object> & objects
   ){
    	camera_utils::msgs::CameraUtilsRequest msg;
    	msg.set_type(CAMERA_POINT_REQUEST);

	for(int j=0;j<objects.size();++j)
	{
		std::pair <BoundingBox3d::iterator, BoundingBox3d::iterator> ret;
		ret = bbs_3d.equal_range(objects[j].name);

		for (BoundingBox3d::iterator it=ret.first; it!=ret.second; ++it)
		{

			ignition::math::Vector3d point_1=it->second.center; point_1.X()+=it->second.size.X()*0.5; point_1.Y()+=it->second.size.Y()*0.5; point_1.Z()+=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_2=it->second.center; point_2.X()+=it->second.size.X()*0.5; point_2.Y()+=it->second.size.Y()*0.5; point_2.Z()-=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_3=it->second.center; point_3.X()+=it->second.size.X()*0.5; point_3.Y()-=it->second.size.Y()*0.5; point_3.Z()+=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_4=it->second.center; point_4.X()+=it->second.size.X()*0.5; point_4.Y()-=it->second.size.Y()*0.5; point_4.Z()-=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_5=it->second.center; point_5.X()-=it->second.size.X()*0.5; point_5.Y()+=it->second.size.Y()*0.5; point_5.Z()+=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_6=it->second.center; point_6.X()-=it->second.size.X()*0.5; point_6.Y()+=it->second.size.Y()*0.5; point_6.Z()-=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_7=it->second.center; point_7.X()-=it->second.size.X()*0.5; point_7.Y()-=it->second.size.Y()*0.5; point_7.Z()+=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_8=it->second.center; point_8.X()-=it->second.size.X()*0.5; point_8.Y()-=it->second.size.Y()*0.5; point_8.Z()-=it->second.size.Z()*0.5;

			/* point 1 */
			gazebo::msgs::Vector3d *point_msg_1 = new gazebo::msgs::Vector3d();
			point_msg_1->set_x(point_1.X());
			point_msg_1->set_y(point_1.Y());
			point_msg_1->set_z(point_1.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_1 = msg.add_bounding_box();
			bounding_box_1->set_name(objects[j].name);
			bounding_box_1->set_allocated_point3d(point_msg_1);

			/* point 2 */
			gazebo::msgs::Vector3d *point_msg_2 = new gazebo::msgs::Vector3d();
			point_msg_2->set_x(point_2.X());
			point_msg_2->set_y(point_2.Y());
			point_msg_2->set_z(point_2.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_2 = msg.add_bounding_box();
			bounding_box_2->set_name(objects[j].name);
			bounding_box_2->set_allocated_point3d(point_msg_2);

			/* point 3 */
			gazebo::msgs::Vector3d *point_msg_3 = new gazebo::msgs::Vector3d();
			point_msg_3->set_x(point_3.X());
			point_msg_3->set_y(point_3.Y());
			point_msg_3->set_z(point_3.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_3 = msg.add_bounding_box();
			bounding_box_3->set_name(objects[j].name);
			bounding_box_3->set_allocated_point3d(point_msg_3);

			/* point 4 */
			gazebo::msgs::Vector3d *point_msg_4 = new gazebo::msgs::Vector3d();
			point_msg_4->set_x(point_4.X());
			point_msg_4->set_y(point_4.Y());
			point_msg_4->set_z(point_4.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_4 = msg.add_bounding_box();
			bounding_box_4->set_name(objects[j].name);
			bounding_box_4->set_allocated_point3d(point_msg_4);

			/* point 5 */
			gazebo::msgs::Vector3d *point_msg_5 = new gazebo::msgs::Vector3d();
			point_msg_5->set_x(point_5.X());
			point_msg_5->set_y(point_5.Y());
			point_msg_5->set_z(point_5.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_5 = msg.add_bounding_box();
			bounding_box_5->set_name(objects[j].name);
			bounding_box_5->set_allocated_point3d(point_msg_5);

			/* point 6 */
			gazebo::msgs::Vector3d *point_msg_6 = new gazebo::msgs::Vector3d();
			point_msg_6->set_x(point_6.X());
			point_msg_6->set_y(point_6.Y());
			point_msg_6->set_z(point_6.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_6 = msg.add_bounding_box();
			bounding_box_6->set_name(objects[j].name);
			bounding_box_6->set_allocated_point3d(point_msg_6);

			/* point 7 */
			gazebo::msgs::Vector3d *point_msg_7 = new gazebo::msgs::Vector3d();
			point_msg_7->set_x(point_7.X());
			point_msg_7->set_y(point_7.Y());
			point_msg_7->set_z(point_7.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_7 = msg.add_bounding_box();
			bounding_box_7->set_name(objects[j].name);
			bounding_box_7->set_allocated_point3d(point_msg_7);

			/* point 8 */
			gazebo::msgs::Vector3d *point_msg_8 = new gazebo::msgs::Vector3d();
			point_msg_8->set_x(point_8.X());
			point_msg_8->set_y(point_8.Y());
			point_msg_8->set_z(point_8.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_8 = msg.add_bounding_box();
			bounding_box_8->set_name(objects[j].name);
			bounding_box_8->set_allocated_point3d(point_msg_8);


		}
	}

    pub->Publish(msg,true);
}

void onWorldUtilsResponse(WorldUtilsResponsePtr &_msg){
    if (_msg->type() == INFO){
        if (_msg->has_object_count()){
            std::lock_guard<std::mutex> lock(object_count_mutex);
            object_count = _msg->object_count();
        }
    } else if (_msg->type() == PROPERTIES){

        for(int i(0); i<_msg->bounding_box_size();++i){
            ignition::math::Vector3d bb_center = gazebo::msgs::ConvertIgn(_msg->bounding_box(i).bb_center());
            ignition::math::Vector3d bb_size = gazebo::msgs::ConvertIgn(_msg->bounding_box(i).bb_size());

            bounding_box_3d bb(bb_center, bb_size);
	    bbs_3d.insert( std::pair<std::string,bounding_box_3d>(_msg->bounding_box(i).name(), bb) );
        }
    }
}

/* Handle camera success */

bool waitForCamera(){

    std::lock_guard<std::mutex> lock(camera_success_mutex);

    if (camera_success){
        camera_success = false;
        return false;
    }
    return true;
}

void onCameraUtilsResponse(CameraUtilsResponsePtr &_msg){
    std::cout << "camera response" << std::endl;
    if (_msg->type()==CAMERA_POINT_RESPONSE){

        for(int i(0); i<_msg->bounding_box_size();++i){
        	ignition::math::Vector2d point_2d = gazebo::msgs::ConvertIgn(_msg->bounding_box(i).point());

    		points_2d.insert( std::pair<std::string,ignition::math::Vector2d>(_msg->bounding_box(i).name(), point_2d) );
	}
    }
    else if (_msg->success()){
	std::cout << "hum2" << std::endl;
        std::lock_guard<std::mutex> lock(camera_success_mutex);
        camera_success = true;    
    } else {
        std::cout << "Camera could not save to file! Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }
}


void storeAnnotations(const std::vector<Object> & objects, const std::string & path, const std::string & file_name)
{
	std::ofstream out(path+file_name);
        out << "<annotation>" << std::endl 
	    << "  <folder>SHAPES2017</folder>" << std::endl
	    << "  <filename>"+file_name+"</filename>" << std::endl
            << "  <source>"<<std::endl
	    << "    <database>The SHAPE2017 Database</database>"<< std::endl
	    << "    <annotation>SHAPE SHAPE2017</annotation>" << std::endl 
            << "    <image>flickr</image>" << std::endl
            << "  </source>" << std::endl
            << "  <size>" << std::endl
            << "    <width></width>" << std::endl
            << "    <height></height>" << std::endl
            << "    <depth></depth>" << std::endl
            << "  </size>" << std::endl
            << "  <segmented>1</segmented>" << std::endl;


	for(unsigned int i=0; i<objects.size(); ++i) 
	{
		out << "  <object>" << std::endl
		    << "    <name>" << classes_map.find(objects[i].type)->second << "</name>" << std::endl
		    << "    <pose></pose>" << std::endl
		    << "    <truncated></truncated>" << std::endl
		    << "    <difficult></difficult>" << std::endl
		    << "    <bndbox>" << std::endl
		    << "      <xmin>"<< objects[i].bounding_box.x <<"</xmin>" << std::endl
		    << "      <ymin>"<< objects[i].bounding_box.y <<"</ymin>" << std::endl
		    << "      <xmax>"<< objects[i].bounding_box.x + objects[i].bounding_box.width <<"</xmax>" << std::endl
		    << "      <ymax>"<< objects[i].bounding_box.x + objects[i].bounding_box.height <<"</ymax>" << std::endl
		    << "    </bndbox>" << std::endl
		    << "  </object>" << std::endl;
	}

	out << "</annotation>";
        out.close();

}
