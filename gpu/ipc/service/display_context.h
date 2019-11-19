// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_DISPLAY_CONTEXT_H_
#define GPU_IPC_SERVICE_DISPLAY_CONTEXT_H_

#include "gpu/ipc/service/gpu_ipc_service_export.h"

namespace gpu {

// Interface for something representing a display compositor context on the GPU
// thread. The interface is used to force context loss if necessary.
class GPU_IPC_SERVICE_EXPORT DisplayContext {
 public:
  virtual ~DisplayContext() = default;

  virtual void MarkContextLost() = 0;
};

}  // namespace gpu

#endif  //  GPU_IPC_SERVICE_DISPLAY_CONTEXT_H_
