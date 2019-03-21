// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MPS_PROTOCOLS_IMPL_H_
#define SERVICES_ML_MPS_PROTOCOLS_IMPL_H_

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#include "services/ml/ml_utils_mac.h"

API_AVAILABLE(macosx(10.13))
@interface ConvDataSource : NSObject <MPSCNNConvolutionDataSource>

@property(nonatomic, assign) float* weights_;

@property(nonatomic, assign) float* bias_;

@property(nonatomic, assign) MPSCNNConvolutionDescriptor* desc_;

- (id)initWithWeight:(float*)weights
                bias:(float*)bias
                desc:(MPSCNNConvolutionDescriptor*)desc;

@end

API_AVAILABLE(macosx(10.13))
@interface CustomPadding : NSObject <MPSNNPadding>

@property(nonatomic, assign) MPSOffset offset_;

@property(nonatomic, assign) MPSImageEdgeMode edge_mode_;

@property(nonatomic, assign) uint32_t num_;

@property(nonatomic, assign) uint32_t width_;

@property(nonatomic, assign) uint32_t height_;

@property(nonatomic, assign) uint32_t channels_;

- (id)initWithOffset:(MPSOffset)offset
            edgeMode:(MPSImageEdgeMode)edgeMode
                 num:(uint32_t)num
               width:(uint32_t)width
              height:(uint32_t)height
            channels:(uint32_t)channels;

@end

API_AVAILABLE(macosx(10.13))
@interface OutputImageAllocator : NSObject <MPSImageAllocator>

@property(nonatomic, retain) MPSImage* image;

@end

@interface TemporaryImageHandle : NSObject <MPSHandle>

@property(nonatomic, copy) NSString* label_;

- (id)initWithLabel:(NSString*)label;

@end

namespace ml {

bool GetMPSImageInfo(const OperandMac& operand,
                     uint32_t& n,
                     uint32_t& width,
                     uint32_t& height,
                     uint32_t& channels);
}

#endif  // SERVICES_ML_MPS_PROTOCOLS_IMPL_H_
