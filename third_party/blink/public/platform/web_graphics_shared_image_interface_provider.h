// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_GRAPHICS_SHARED_IMAGE_INTERFACE_PROVIDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_GRAPHICS_SHARED_IMAGE_INTERFACE_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/platform/web_common.h"

namespace gpu {
class SharedImageInterface;
}  // namespace gpu

namespace blink {

// This class provides the ClientSharedImageInterface for Canvas Resource to
// create shared images in software rendering mode. Canvas Resource cannot get
// ClientSharedImageInterface from WebGraphicsContext3DProvider because it's
// only available in GPU mode.
class BLINK_PLATFORM_EXPORT WebGraphicsSharedImageInterfaceProvider {
 public:
  virtual ~WebGraphicsSharedImageInterfaceProvider() = default;

  virtual void SetLostGpuChannelCallback(base::RepeatingClosure task) = 0;

  // When the GPU channel is lost, SharedImageInterface should returns nullptr.
  virtual gpu::SharedImageInterface* SharedImageInterface() = 0;

  virtual base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
  GetWeakPtr() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_GRAPHICS_SHARED_IMAGE_INTERFACE_PROVIDER_H_
