// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_IOS_BE_LAYER_HIERARCHY_TRANSPORT_H_
#define GPU_IPC_COMMON_IOS_BE_LAYER_HIERARCHY_TRANSPORT_H_

#include <xpc/xpc.h>

#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "gpu/ipc/common/surface_handle.h"

namespace gpu {

// Allows the BELayerHierarchy to be transported from the GPU process
// to the browser process. Since BELayerHierarchy is only serializable
// over XPC (and not mojo) it needs a hook into the XPC IPC channel.
class GPU_IPC_COMMON_EXPORT BELayerHierarchyTransport {
 public:
  static BELayerHierarchyTransport* GetInstance();
  static void SetInstance(BELayerHierarchyTransport* instance);

  // Sends the BELayerHierarchy represented as an xpc_object_t that
  // is associated with `surface_handle` to the browser process.
  virtual void ForwardBELayerHierarchyToBrowser(
      SurfaceHandle surface_handle,
      xpc_object_t ipc_representation) = 0;

 protected:
  virtual ~BELayerHierarchyTransport() {}
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_IOS_BE_LAYER_HIERARCHY_TRANSPORT_H_
