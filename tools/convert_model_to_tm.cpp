/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * License); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * AS IS BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Copyright (c) 2020, OPEN AI LAB
 * Author: bzhang@openailab.com
 */

#include "config.hpp"

#include <stdlib.h>
#include <iostream>
#include <unistd.h>

#include "tengine_c_api.h"

void show_usage()
{
    std::cout << "[Usage]: [-h] [-f file_format] [-p proto_file] [-m model_file] [-o output_tmfile]\n";
}

int main(int argc, char* argv[])
{
    std::string file_format;
    std::string proto_file;
    std::string model_file;
    std::string output_tmfile;
    bool proto_file_needed = false;
    bool model_file_needed = false;
    int input_file_number = 0;

    int res;
    while ((res = getopt(argc, argv, "f:p:m:o:h")) != -1)
    {
        switch (res)
        {
            case 'f':
                file_format = optarg;
                break;
            case 'p':
                proto_file = optarg;
                break;
            case 'm':
                model_file = optarg;
                break;
            case 'o':
                output_tmfile = optarg;
                break;
            case 'h':
                show_usage();
                return 0;
            default:
                show_usage();
                break;
        }
    }

    // Check the input parameters

    if (file_format.empty())
    {
        show_usage();
        return -1;
    }
    else
    {
        if (file_format == "caffe" || file_format == "mxnet" || file_format == "darknet" || file_format == "ncnn")
        {
            proto_file_needed = true;
            model_file_needed = true;
            input_file_number = 2;
        }
#ifdef BUILD_MEGENGINE_SERIALIZER
        else if (file_format == "caffe_single" || file_format == "onnx" || file_format == "tensorflow" ||
                 file_format == "tflite" || file_format == "megengine")
#else
        else if (file_format == "caffe_single" || file_format == "onnx" || file_format == "tensorflow" ||
                 file_format == "tflite")
#endif
        {
            model_file_needed = true;
            input_file_number = 1;
        }
        else
        {
#ifdef BUILD_MEGENGINE_SERIALIZER
            std::cout << "Allowed input file format: caffe, caffe_single, onnx, mxnet, tensorflow, darknet, ncnn, megengine\n";
#else
            std::cout << "Allowed input file format: caffe, caffe_single, onnx, mxnet, tensorflow, darknet, ncnn\n";
#endif
            return -1;
        }
    }

    if (proto_file_needed)
    {
        if (proto_file.empty())
        {
            std::cout << "Please specify the -p option to indicate the input proto file.\n";
            return -1;
        }
        if (access(proto_file.c_str(), 0) == -1)
        {
            std::cout << "Proto file does not exist: " << proto_file << "\n";
            return -1;
        }
    }

    if (model_file_needed)
    {
        if (model_file.empty())
        {
            std::cout << "Please specify the -m option to indicate the input model file.\n";
            return -1;
        }
        if (access(model_file.c_str(), 0) == -1)
        {
            std::cout << "Model file does not exist: " << model_file << "\n";
            return -1;
        }
    }

    if (output_tmfile.empty())
    {
        std::cout << "Please specify the -o option to indicate the output tengine model file.\n";
        return -1;
    }
    if (output_tmfile.rfind("/") != std::string::npos)
    {
        std::string output_dir = output_tmfile.substr(0, output_tmfile.rfind("/"));
        if (access(output_dir.c_str(), 0) == -1)
        {
            std::cout << "The dir of output file does not exist: " << output_dir << "\n";
            return -1;
        }
    }

    // init tengine
    init_tengine();

    // create graph
    graph_t graph = nullptr;
    if (input_file_number == 2)
        graph = create_graph(nullptr, file_format.c_str(), proto_file.c_str(), model_file.c_str());
    else if (input_file_number == 1)
        graph = create_graph(nullptr, file_format.c_str(), model_file.c_str());

    if (graph == nullptr)
    {
        std::cout << "Create graph failed\n";
        // std::cout << "errno: " << get_tengine_errno() << "\n";
        return -1;
    }

    const char* env = std::getenv("TM_NO_OPTIMIZE");
    if (env == nullptr)
    {
        // optimize graph
        int optimize_only = 1;
        if (set_graph_attr(graph, "optimize_only", &optimize_only, sizeof(int)) < 0)
        {
            std::cout << "set optimize only failed\n";
            return -1;
        }

        if (prerun_graph(graph) < 0)
        {
            std::cout << "prerun failed\n";
            return -1;
        }
    }

    // Save the tengine model file
    if (save_graph(graph, "tengine", output_tmfile.c_str()) == -1)
    {
        std::cout << "Create tengine model file failed.\n";
        return -1;
    }
    std::cout << "Create tengine model file done: " << output_tmfile << "\n";

    destroy_graph(graph);
    release_tengine();

    return 0;
}
