// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_POOL_SERVICE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_POOL_SERVICE_H_

#include "gpu/command_buffer/common/shared_image_pool_id.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/shared_image_pool_client_interface.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace gpu {
class SharedImageFactory;

// This class is the GPU service-side counterpart of the corresponding
// SharedImagePool on the client-side(renderer/browser). It is responsible for
// various service side optimizations for all the shared image backings
// belonging to this pool. It is also responsible for communicating various
// signals to the corresponding client-side pool via a dedicated IPC channel.
class GPU_GLES2_EXPORT SharedImagePoolService {
 public:
  SharedImagePoolService(
      SharedImagePoolId pool_id,
      mojo::PendingRemote<mojom::SharedImagePoolClientInterface> client_remote,
      SharedImageFactory* factory);
  ~SharedImagePoolService();

  // Triggers the OnClearPool method on the client-side interface.
  void NotifyClearPool();

  SharedImagePoolId GetPoolId() const { return pool_id_; }

 private:
  // unique unguessable id to identify this pool.
  const SharedImagePoolId pool_id_;

  // Mojo connection which will be used to send IPC to SharedImagePool in the
  // client process.
  mojo::Remote<mojom::SharedImagePoolClientInterface> client_remote_;

  // Note that SharedImageFactory will own and outlive SharedImagePoolService.
  raw_ptr<SharedImageFactory> factory_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_POOL_SERVICE_H_
