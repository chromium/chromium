
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MPSCNNCONTEXT_H_
#define SERVICES_ML_MPSCNNCONTEXT_H_

#import <Metal/MTLBuffer.h>
#import <Metal/MTLDevice.h>
#import <Metal/MTLLibrary.h>

namespace ml {

struct API_AVAILABLE(macosx(10.11)) MPSCNNContext {
 public:
  MPSCNNContext() : initialized(false) {}
  bool initialized;
  id<MTLDevice> device;
  id<MTLCommandQueue> command_queue;
  id<MTLLibrary> library;
};

MPSCNNContext& API_AVAILABLE(macosx(10.11)) GetMPSCNNContext();

}

#endif  // SERVICES_ML_MPSCNNCONTEXT_H_