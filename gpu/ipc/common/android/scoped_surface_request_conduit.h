// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_ANDROID_SCOPED_SURFACE_REQUEST_CONDUIT_H_
#define GPU_IPC_COMMON_ANDROID_SCOPED_SURFACE_REQUEST_CONDUIT_H_

#include "gpu/gpu_export.h"

namespace base {
class UnguessableToken;
}

namespace gpu {
class TextureOwner;

// Allows the forwarding of TextureOwners from the GPU or the browser process
// to fulfill requests registered by the ScopedSurfaceRequestManager.
class GPU_EXPORT ScopedSurfaceRequestConduit {
 public:
  static ScopedSurfaceRequestConduit* GetInstance();
  static void SetInstance(ScopedSurfaceRequestConduit* instance);

  // Sends the surface owner to the ScopedSurfaceRequestManager in the browser
  // process, to fulfill the request registered under the |request_token| key.
  virtual void ForwardSurfaceOwnerForSurfaceRequest(
      const base::UnguessableToken& request_token,
      const TextureOwner* texture_owner) = 0;

 protected:
  virtual ~ScopedSurfaceRequestConduit() {}
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_ANDROID_SCOPED_SURFACE_REQUEST_CONDUIT_H_
