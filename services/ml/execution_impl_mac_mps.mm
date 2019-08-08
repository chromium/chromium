// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_mac_mps.h"

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"
#include "services/ml/ml_utils_mac.h"
#include "services/ml/mps_protocols_impl.h"
#include "services/ml/mpscnn_context.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

namespace {

NSString* API_AVAILABLE(macosx(10.13)) KernelFor(const MPSImage* X,
                                                 NSString* arrayKernel,
                                                 NSString* nonArrayKernel) {
  if (X.featureChannels > 4) {
    return arrayKernel;
  }
  if (X.numberOfImages > 1) {
    return arrayKernel;
  }
  return nonArrayKernel;
}

auto divRoundUp(uint x, uint y) -> uint {
  return (x + y - 1) / y;
}

struct LaunchParams {
  MTLSize threadsPerThreadgroup;
  MTLSize threadgroupsPerGrid;
};

LaunchParams API_AVAILABLE(macosx(10.13))
    SpatialPointwiseKernelLaunchParams(id<MTLComputePipelineState> pipeline,
                                       const MPSImage* im) {
  // const auto maxThreadsPerThreadgroup =
  //[pipeline maxTotalThreadsPerThreadgroup];
  // const auto threadExecutionWidth = [pipeline threadExecutionWidth];
  const auto threadsPerThreadgroup =
      MTLSizeMake(8 /* threadExecutionWidth */,
                  4 /* maxThreadsPerThreadgroup / threadExecutionWidth */, 1);
  const auto threadgroupsPerGrid =
      MTLSizeMake(divRoundUp(im.width, threadsPerThreadgroup.width),
                  divRoundUp(im.height, threadsPerThreadgroup.height),
                  im.numberOfImages * divRoundUp(im.featureChannels, 4));
  return {threadsPerThreadgroup, threadgroupsPerGrid};
}

MPSImageDescriptor* API_AVAILABLE(macosx(10.13))
    CreateMPSImageDescriptor(const OperandMac& operand) {
  int32_t type = operand.type;
  MPSImageDescriptor* mpsimage_desc = nullptr;
  if (type != mojom::TENSOR_FLOAT32) {
    DLOG(ERROR) << "type " << type << " is not supported";
    return mpsimage_desc;
  }
  uint32_t n, width, height, channels;
  if (!ml::GetMPSImageInfo(operand, n, width, height, channels)) {
    return mpsimage_desc;
  }
  mpsimage_desc = [MPSImageDescriptor
      imageDescriptorWithChannelFormat:MPSImageFeatureChannelFormatFloat16
                                 width:width
                                height:height
                       featureChannels:channels
                        numberOfImages:n
                                 usage:MTLTextureUsageShaderRead |
                                       MTLTextureUsageShaderWrite];
  LOG(INFO) << "Create MPSImageDescriptor " << mpsimage_desc << " [" << width
            << ", " << height << ", " << channels << "]";
  return mpsimage_desc;
}

API_AVAILABLE(macosx(10.13))
void SaveTemporaryImages(std::map<uint32_t, MPSImage*>& temporary_images,
                         const NSMutableArray<MPSImage*>* intermediate_images) {
  for (MPSImage* image in intermediate_images) {
    uint32_t input_index = [image.label intValue];
    temporary_images[input_index] = image;
  }
}

}  // namespace

ExecutionImplMacMPS::ExecutionImplMacMPS(
    base::WeakPtr<CompilationImplMac> compilation,
    mojo::ScopedSharedBufferHandle memory) {
  compilation_ = compilation;
  uint32_t mapped_length = 0;
  SetupOperandInfoForOperands(inputs_info_, compilation_->operands_,
                              compilation_->inputs_, memory, mapped_length);
  SetupOperandInfoForOperands(outputs_info_, compilation_->operands_,
                              compilation_->outputs_, memory, mapped_length);

  if (@available(macOS 10.13, *)) {
    SetupMPSImageForOperands(input_mpsimages_, input_mtlbuffers_,
                             compilation_->inputs_);
    SetupMPSImageForOperands(constant_mpsimages_, constant_mtlbuffers_,
                             compilation_->constants_);
    CreateOutputMTLBuffer();
  }
}

ExecutionImplMacMPS::~ExecutionImplMacMPS() = default;

bool ExecutionImplMacMPS::IsValid() const {
  bool valid = compilation_ != nil &&
               inputs_info_.size() == compilation_->inputs_.size() &&
               outputs_info_.size() == compilation_->outputs_.size();
  if (compilation_) {
    if (@available(macOS 10.13, *)) {
      valid &= compilation_->inputs_.size() == input_mpsimages_.size() &&
               compilation_->constants_.size() == constant_mpsimages_.size();
    }
  }
  return valid;
}

void API_AVAILABLE(macosx(10.13)) ExecutionImplMacMPS::SetupMPSImageForOperands(
    std::vector<base::scoped_nsobject<MPSImage>>& mps_image_array,
    std::vector<id<MTLBuffer>>& mtl_buffer_array,
    const std::vector<uint32_t>& operands_index_array) {
  for (size_t i = 0; i < operands_index_array.size(); ++i) {
    const OperandMac& operand =
        compilation_->operands_[operands_index_array[i]];
    if (@available(macOS 10.13, *)) {
      MPSImageDescriptor* descriptor = CreateMPSImageDescriptor(operand);
      if (!descriptor)
        return;
      base::scoped_nsobject<MPSImage> mps_img([[MPSImage alloc]
           initWithDevice:GetMPSCNNContext().device
          imageDescriptor:descriptor]);
      mps_image_array.push_back(std::move(mps_img));
      mtl_buffer_array.push_back([GetMPSCNNContext().device
          newBufferWithLength:operand.requiredSize()
                      options:MTLResourceOptionCPUCacheModeWriteCombined]);
    }
  }
}

API_AVAILABLE(macosx(10.13))
void ExecutionImplMacMPS::CreateOutputMTLBuffer() {
  for (size_t i = 0; i < compilation_->outputs_.size(); ++i) {
    const OperandMac& operand =
        compilation_->operands_[compilation_->outputs_[i]];
    output_mtlbuffers_.push_back([GetMPSCNNContext().device
        newBufferWithLength:operand.requiredSize()
                    options:MTLResourceOptionCPUCacheModeWriteCombined]);
  }
}

void ExecutionImplMacMPS::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplMac::StartCompute";
  bool success = true;
  if (@available(macOS 10.13, *)) {
    do {
      @autoreleasepool {
        id<MTLCommandBuffer> command_buffer =
            [GetMPSCNNContext().command_queue commandBuffer];

        NSMutableArray<MPSImage*>* image_array =
            [NSMutableArray arrayWithCapacity:1];
        for (size_t i = 0; i < compilation_->inputs_.size(); ++i) {
          std::unique_ptr<OperandInfo>& input_data = inputs_info_[i];
          MPSImage* mps_img = input_mpsimages_[i].get();
          const id<MTLBuffer> mtl_buffer = input_mtlbuffers_[i];
          UploadToMPSImage(mps_img, mtl_buffer, command_buffer,
                           input_data->mapping.get(), input_data->length);
          [image_array addObject:mps_img];
        }

        for (size_t i = 0; i < compilation_->constants_.size(); ++i) {
          uint32_t index = compilation_->constants_[i];
          if (compilation_->values_.find(index) ==
              compilation_->values_.end()) {
            DLOG(ERROR) << "Can't find constant " << index;
            success = false;
            break;
          }
          const ValueInfo& value_info =
              compilation_->values_[compilation_->constants_[i]];
          MPSImage* mps_img = constant_mpsimages_[i].get();
          const id<MTLBuffer> mtl_buffer = constant_mtlbuffers_[i];
          const void* cpu_buffer = static_cast<const void*>(
              compilation_->memory_.get() + value_info.offset);
          UploadToMPSImage(mps_img, mtl_buffer, command_buffer, cpu_buffer,
                           value_info.length);
          [image_array addObject:mps_img];
        }

        std::map<uint32_t, MPSImage*> output_mps_images;
        std::map<uint32_t, MPSImage*> temporary_mps_images;
        for (size_t i = 0; i < compilation_->graphs_.size(); i++) {
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
                compilation_->graphs_[i].get().sourceImageHandles;
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

          MPSImage* graph_output_image = [compilation_->graphs_[i]
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

        for (size_t i = 0; i < compilation_->outputs_.size(); ++i) {
          MPSImage* output_img = output_mps_images[compilation_->outputs_[i]];
          id<MTLBuffer> output_buffer = output_mtlbuffers_[i];

          id<MTLComputeCommandEncoder> encoder =
              [command_buffer computeCommandEncoder];
          id<MTLComputePipelineState> state =
              GetMPSCNNContext().GetSpecializedPipelineState(
                  KernelFor(output_img, @"copy_metal_to_nhwc",
                            @"copy_metal_to_nhwc_nonarray"),
                  {{ushort(output_img.height), ushort(output_img.width),
                    ushort(output_img.featureChannels)}});

          [encoder setComputePipelineState:state];
          [encoder setBuffer:output_buffer offset:0 atIndex:0];
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

        for (size_t i = 0; i < compilation_->outputs_.size(); ++i) {
          std::unique_ptr<OperandInfo>& output_data = outputs_info_[i];
          id<MTLBuffer> output_buffer = output_mtlbuffers_[i];
          memcpy(output_data->mapping.get(), [output_buffer contents],
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

void ExecutionImplMacMPS::UploadToMPSImage(
    const MPSImage* mps_image,
    const id<MTLBuffer>& mtl_buffer,
    const id<MTLCommandBuffer>& command_buffer,
    const void* cpu_buffer,
    size_t length) {
  if (@available(macOS 10.13, *)) {
    memcpy([mtl_buffer contents], cpu_buffer, length);
    id<MTLComputeCommandEncoder> encoder =
        [command_buffer computeCommandEncoder];
    id<MTLComputePipelineState> state =
        GetMPSCNNContext().GetSpecializedPipelineState(
            KernelFor(mps_image, @"copy_nhwc_to_metal",
                      @"copy_nhwc_to_metal_nonarray"),
            {{ushort(mps_image.height), ushort(mps_image.width),
              ushort(mps_image.featureChannels)}});
    [encoder setComputePipelineState:state];
    [encoder setBuffer:mtl_buffer offset:0 atIndex:0];
    [encoder setTexture:[mps_image texture] atIndex:0];
    const auto& inputLaunchParams =
        SpatialPointwiseKernelLaunchParams(state, mps_image);
    [encoder dispatchThreadgroups:inputLaunchParams.threadgroupsPerGrid
            threadsPerThreadgroup:inputLaunchParams.threadsPerThreadgroup];
    [encoder endEncoding];
  }
}

}  // namespace ml
