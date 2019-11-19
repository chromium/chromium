// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DELEGATE_PROVIDER_H_
#define DEVICE_VR_ANDROID_GVR_DELEGATE_PROVIDER_H_

#include "base/macros.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"

namespace device {

// TODO(mthiesse, crbug.com/769373): Remove this interface and replace with a
// mojo interface.
class DEVICE_VR_EXPORT GvrDelegateProvider {
 public:
  GvrDelegateProvider() = default;
  virtual bool ShouldDisableGvrDevice() = 0;
  virtual void StartWebXRPresentation(
      mojom::VRDisplayInfoPtr display_info,
      mojom::XRRuntimeSessionOptionsPtr options,
      base::OnceCallback<void(device::mojom::XRSessionPtr)> callback) = 0;
  virtual void ExitWebVRPresent() = 0;

 protected:
  virtual ~GvrDelegateProvider() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(GvrDelegateProvider);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DELEGATE_PROVIDER_H_
