// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_mps.h"

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"
#include "services/ml/common.h"
#include "services/ml/ml_utils_mac.h"
#include "services/ml/mps_protocols_impl.h"
#include "services/ml/mpscnn_context.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

namespace {

API_AVAILABLE(macosx(10.13))
void PrepareMemoryForReusing(
    std::vector<std::unique_ptr<OperandInfo>>& reuse_memory,
    std::vector<id<MTLBuffer>>& reuse_buffers,
    const std::vector<ml::mojom::OperandInfoPtr>& user_data,
    mojo::ScopedSharedBufferHandle& memory,
    uint32_t& mapped_length) {
  for (size_t i = 0; i < user_data.size(); ++i) {
    const mojom::OperandInfoPtr& operand = user_data[i];
    uint32_t length = GetRequiredSize(operand);
    mojo::ScopedSharedBufferMapping mapping =
        memory->MapAtOffset(length, mapped_length);
    std::unique_ptr<OperandInfo> info(
        new OperandInfo(mapped_length, length, std::move(mapping)));
    reuse_memory.push_back(std::move(info));
    mapped_length += length;

    reuse_buffers.push_back([GetMPSCNNContext().device
        newBufferWithLength:length
                    options:MTLResourceOptionCPUCacheModeWriteCombined]);
  }
}

NSString* API_AVAILABLE(macosx(10.13))
    OutputKernel(const mojom::OperandInfoPtr& operand,
                 const MPSImage* output_img) {
  NSString* kernel = nullptr;
  if (operand->type == mojom::TENSOR_FLOAT32) {
    kernel = KernelFor(output_img, @"copy_metal_to_nhwc",
                       @"copy_metal_to_nhwc_nonarray");
  } else if (operand->type == mojom::TENSOR_INT32) {
    kernel = KernelFor(output_img, @"output_nhwc_int_data",
                       @"output_nhwc_int_data_nonarray");
  } else {
    LOG(ERROR) << "The output data type isn't supported.";
  }
  return kernel;
}

API_AVAILABLE(macosx(10.13))
void SaveTemporaryImages(std::map<uint32_t, MPSImage*>& temporary_images,
                         const NSMutableArray<MPSImage*>* intermediate_images) {
  for (MPSImage* image in intermediate_images) {
    uint32_t input_index = [image.label intValue];
    temporary_images[input_index] = image;
  }
}

API_AVAILABLE(macosx(10.13))
MPSImage* GetMPSImage(NSArray<MPSImageHandle*>* handles, uint32_t index) {
  for (size_t i = 0; i < handles.count; ++i) {
    if (handles[i].index == index)
      return handles[i].image;
  }
  return nullptr;
}

}  // namespace

API_AVAILABLE(macosx(10.13))
ExecutionImplMPS::ExecutionImplMPS(
    scoped_refptr<CompiledModelMPS> compiled_model,
    mojom::ExecutionInitParamsPtr params)
    : params_(std::move(params)), compiled_model_(std::move(compiled_model)) {
  uint32_t mapped_length = 0;
  PrepareMemoryForReusing(inputs_info_, input_mtlbuffers_, params_->inputs,
                          params_->memory, mapped_length);
  PrepareMemoryForReusing(outputs_info_, output_mtlbuffers_, params_->outputs,
                          params_->memory, mapped_length);
}

ExecutionImplMPS::~ExecutionImplMPS() = default;

void ExecutionImplMPS::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplMPS::StartCompute";
  bool success = true;
  if (@available(macOS 10.13, *)) {
    do {
      @autoreleasepool {
        id<MTLCommandBuffer> command_buffer =
            [GetMPSCNNContext().command_queue commandBuffer];
        NSArray<MPSImageHandle*>* handles =
            compiled_model_->graphs_[0].get().sourceImageHandles;
        for (size_t i = 0; i < params_->inputs.size(); ++i) {
          std::unique_ptr<OperandInfo>& input_data = inputs_info_[i];
          const mojom::OperandInfoPtr& operand = params_->inputs[i];
          MPSImage* mps_image = GetMPSImage(handles, operand->index);
          if (!mps_image) {
            LOG(ERROR) << "Failed getting MPSImage for inputs data.";
            success = false;
            break;
          }
          UploadToMPSImage(mps_image, input_mtlbuffers_[i], command_buffer,
                           input_data->mapping.get(), input_data->length);
        }

        NSMutableArray<MPSImage*>* image_array =
            [NSMutableArray arrayWithCapacity:1];
        for (size_t i = 0; i < handles.count; ++i) {
          [image_array addObject:handles[i].image];
        }

        std::map<uint32_t, MPSImage*> output_mps_images;
        std::map<uint32_t, MPSImage*> temporary_mps_images;
        for (size_t i = 0; i < compiled_model_->graphs_.size(); i++) {
          // temporary_inputs_[i -1] is the temporary input image index.
          // temporary_mps_images[temporary_inputs_[i -1]] is temporary input
          // image.
          NSMutableArray<MPSImage*>* source_images;
          if (i == 0) {
            // image_array is First graph
            source_images = image_array;
          } else {
            source_images = [NSMutableArray arrayWithCapacity:1];
            NSArray<id<MPSHandle>>* source_image_handles =
                compiled_model_->graphs_[i].get().sourceImageHandles;
            if (source_image_handles.count) {
              // There are only one paramters for new graph.
              DCHECK(source_image_handles.count == 1);
              uint32_t input_index = [source_image_handles[0].label intValue];
              // Find temporary input images of next graph.
              // The node has been optimized by last graph that isn't result
              // node, so the value of temporary_mps_images[input_index] will
              // be null, the desnet need to be support above 10.15 with
              // resultImages.
              // such as graph in desnet: 112->126->122 126->3->0, the 122 node
              // will be optimized.
              if (temporary_mps_images.find(input_index) ==
                  temporary_mps_images.end())
                break;
              [source_images addObject:temporary_mps_images[input_index]];
            }
          }
          NSMutableArray<MPSImage*>* intermediate_images =
              [NSMutableArray arrayWithCapacity:1];

          MPSImage* graph_output_image = [compiled_model_->graphs_[i]
              encodeToCommandBuffer:command_buffer
                       sourceImages:source_images
                       sourceStates:nullptr
                 intermediateImages:intermediate_images
                  destinationStates:nullptr];

          SaveTemporaryImages(temporary_mps_images, intermediate_images);
          // The order of graph is not the same as compilation_->output_.
          uint32_t output_index = [graph_output_image.label intValue];
          output_mps_images[output_index] = graph_output_image;
        }

        for (size_t i = 0; i < params_->outputs.size(); ++i) {
          const mojom::OperandInfoPtr& operand = params_->outputs[i];
          size_t output_index = operand->index;
          MPSImage* output_img = output_mps_images[output_index];
          id<MTLComputePipelineState> state =
              GetMPSCNNContext().GetSpecializedPipelineState(
                  OutputKernel(operand, output_img),
                  {{ushort(output_img.height), ushort(output_img.width),
                    ushort(output_img.featureChannels)}});

          id<MTLComputeCommandEncoder> encoder =
              [command_buffer computeCommandEncoder];
          [encoder setComputePipelineState:state];
          [encoder setBuffer:output_mtlbuffers_[i] offset:0 atIndex:0];
          [encoder setTexture:[output_img texture] atIndex:0];

          const auto& outputLaunchParams =
              SpatialPointwiseKernelLaunchParams(state, output_img);
          [encoder
               dispatchThreadgroups:outputLaunchParams.threadgroupsPerGrid
              threadsPerThreadgroup:outputLaunchParams.threadsPerThreadgroup];
          [encoder endEncoding];
        }

        [command_buffer commit];
        [command_buffer waitUntilCompleted];

        // Read result back.
        for (size_t i = 0; i < params_->outputs.size(); ++i) {
          std::unique_ptr<OperandInfo>& output_data = outputs_info_[i];
          memcpy(output_data->mapping.get(), [output_mtlbuffers_[i] contents],
                 output_data -> length);
        }
      }  // @autoreleasepool
    } while (0);
  }

  if (success) {
    std::move(callback).Run(mojom::NOT_ERROR);
  } else {
    std::move(callback).Run(mojom::BAD_DATA);
  }
}

}  // namespace ml
