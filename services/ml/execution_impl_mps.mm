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

NSString* API_AVAILABLE(macosx(10.13))
    OutputKernel(const OperandMac& operand, const MPSImage* output_img) {
  NSString* kernel = nullptr;
  if (operand.type == mojom::TENSOR_FLOAT32) {
    kernel = KernelFor(output_img, @"copy_metal_to_nhwc",
                       @"copy_metal_to_nhwc_nonarray");
  } else if (operand.type == mojom::TENSOR_INT32) {
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
  for (size_t i = 0; i < compiled_model_->outputs_.size(); ++i) {
    const OperandMac& operand =
        compiled_model_->operands_[compiled_model_->outputs_[i]];
    output_mtlbuffers_.push_back([GetMPSCNNContext().device
        newBufferWithLength:operand.requiredSize()
                    options:MTLResourceOptionCPUCacheModeWriteCombined]);
  }
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
        uint32_t memory_offset = 0;
        for (size_t i = 0; i < params_->inputs.size(); ++i) {
          const mojom::OperandInfoPtr& operand = params_->inputs[i];
          uint32_t offset = memory_offset;
          uint32_t length = GetRequiredSize(operand);
          memory_offset += length;
          auto mapping = params_->memory->MapAtOffset(length, offset);
          MPSImage* mps_image = GetMPSImage(handles, operand->index);
          if (!mps_image) {
            LOG(ERROR) << "Failed getting MPSImage for inputs data.";
            success = false;
            break;
          }
          UploadToMPSImage(mps_image, command_buffer,
                           static_cast<void*>(mapping.get()), length);
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

        for (size_t i = 0; i < compiled_model_->outputs_.size(); ++i) {
          size_t output_index = compiled_model_->outputs_[i];
          MPSImage* output_img = output_mps_images[output_index];
          id<MTLComputePipelineState> state =
              GetMPSCNNContext().GetSpecializedPipelineState(
                  OutputKernel(compiled_model_->operands_[output_index],
                               output_img),
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

        ReadResultBack(memory_offset);
      }  // @autoreleasepool
    } while (0);
  }

  if (success) {
    std::move(callback).Run(mojom::NOT_ERROR);
  } else {
    std::move(callback).Run(mojom::BAD_DATA);
  }
}

void ExecutionImplMPS::ReadResultBack(uint32_t memory_offset) {
  for (size_t i = 0; i < params_->outputs.size(); ++i) {
    const mojom::OperandInfoPtr& operand = params_->outputs[i];
    const uint32_t offset = memory_offset;
    const uint32_t output_buffer_size = GetRequiredSize(operand);
    memory_offset += output_buffer_size;
    auto mapping = params_->memory->MapAtOffset(output_buffer_size, offset);
    memcpy(mapping.get(), [output_mtlbuffers_[i] contents], output_buffer_size);
  }
}

}  // namespace ml
