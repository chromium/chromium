// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/mpscnn_context.h"

namespace ml {

MPSCNNContext& GetMPSCNNContext() {
  static MPSCNNContext ctx;
  if (ctx.initialized == false) {
  	ctx.device = MTLCreateSystemDefaultDevice();
  	ctx.command_queue = [ctx.device newCommandQueue];
  	ctx.initialized = true;
  };
  return ctx;
}

}