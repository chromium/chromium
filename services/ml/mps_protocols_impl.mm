// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/mps_protocols_impl.h"

#include <vector>

#include "base/logging.h"
#include "services/ml/mpscnn_context.h"

@implementation ConvDataSource

@synthesize weights_;

@synthesize bias_;

@synthesize desc_;

- (id)initWithWeight:(float*)weights
                bias:(float*)bias
                desc:(MPSCNNConvolutionDescriptor*)desc {
  self = [super init];
  self.weights_ = weights;
  self.bias_ = bias;
  self.desc_ = desc;
  return self;
}

- (float*)biasTerms {
  return self.bias_;
}

- (MPSDataType)dataType {
  return MPSDataTypeFloat32;
}

- (MPSCNNConvolutionDescriptor*)descriptor {
  return self.desc_;
}

- (NSString*)label {
  return nullptr;
}

- (BOOL)load {
  return true;
}

- (float*)lookupTableForUInt8Kernel {
  return nullptr;
}

- (void)purge {
  return;
}

- (vector_float2*)rangesForUInt8Kernel {
  return nullptr;
}

- (void*)weights {
  return self.weights_;
}

- (id)copyWithZone:(struct _NSZone*)zone {
  ConvDataSource* source = [[ConvDataSource allocWithZone:zone] init];
  source.weights_ = self.weights_;
  source.bias_ = self.bias_;
  source.desc_ = self.desc_;
  return source;
}

@end

@implementation CustomPadding

@synthesize offset_;

@synthesize edge_mode_;

@synthesize num_;

@synthesize width_;

@synthesize height_;

@synthesize channels_;

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (id)initWithCoder:(NSCoder*)coder {
  self = [super init];
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
}

- (id)initWithOffset:(MPSOffset)offset
            edgeMode:(MPSImageEdgeMode)edgeMode
                 num:(uint32_t)num
               width:(uint32_t)width
              height:(uint32_t)height
            channels:(uint32_t)channels {
  self = [super init];
  self.offset_ = offset;
  self.edge_mode_ = edgeMode;
  self.num_ = num;
  self.width_ = width;
  self.height_ = height;
  self.channels_ = channels;
  return self;
}

- (MPSNNPaddingMethod)paddingMethod {
  return MPSNNPaddingMethodCustom;
}

- (MPSImageDescriptor*)
    destinationImageDescriptorForSourceImages:(NSArray<MPSImage*>*)sourceImages
                                 sourceStates:(NSArray<MPSState*>*)sourceStates
                                    forKernel:(MPSKernel*)kernel
                          suggestedDescriptor:
                              (MPSImageDescriptor*)inDescriptor {
  if ([kernel isKindOfClass:[MPSCNNKernel class]]) {
    MPSCNNKernel* cnn_kernel = (MPSCNNKernel*)kernel;
    [cnn_kernel setOffset:offset_];
    [cnn_kernel setEdgeMode:edge_mode_];
  }

  [inDescriptor setChannelFormat:MPSImageFeatureChannelFormatFloat16];
  [inDescriptor setNumberOfImages:num_];
  [inDescriptor setWidth:width_];
  [inDescriptor setHeight:height_];
  [inDescriptor setFeatureChannels:channels_];
  [inDescriptor
      setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];

  return inDescriptor;
}

@end

@implementation OutputImageAllocator

@synthesize image = _image;

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (id)initWithCoder:(NSCoder*)coder {
  self = [super init];
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
}

- (MPSImage*)imageForCommandBuffer:(id<MTLCommandBuffer>)cmdBuf
                   imageDescriptor:(MPSImageDescriptor*)descriptor
                            kernel:(MPSKernel*)kernel {
  if (self.image)
    return self.image;

  self.image = [[MPSImage alloc] initWithDevice:ml::GetMPSCNNContext().device
                                imageDescriptor:descriptor];

  return self.image;
}

@end

@implementation TemporaryImageHandle

@synthesize label_ = _label;

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (id)initWithCoder:(NSCoder*)coder {
  self = [super init];
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
}

- (id)initWithLabel:(NSString*)label {
  self = [super init];
  self.label_ = label;
  return self;
}

/*! @abstract   A label to be attached to associated MTLResources for this node
 *  @return     A human readable string for debugging purposes
 */
- (NSString*)label {
  return self.label_;
}

@end

namespace ml {

bool GetMPSImageInfo(const OperandMac& operand,
                     uint32_t& n,
                     uint32_t& width,
                     uint32_t& height,
                     uint32_t& channels) {
  const std::vector<uint32_t>& dimensions = operand.dimensions;
  if (dimensions.size() == 4) {
    n = dimensions[0];
    height = dimensions[1];
    width = dimensions[2];
    channels = dimensions[3];
    return true;
  } else if (dimensions.size() == 3) {
    n = 1;
    height = dimensions[0];
    width = dimensions[1];
    channels = dimensions[2];
    return true;
  } else if (dimensions.size() == 2) {
    n = 1;
    height = 1;
    width = dimensions[0];
    channels = dimensions[1];
    return true;
  } else if (dimensions.size() == 1) {
    n = 1;
    height = 1;
    width = 1;
    channels = dimensions[0];
    return true;
  } else {
    DLOG(ERROR) << "dimension " << dimensions.size() << " is not supported";
    return false;
  }
}
}
