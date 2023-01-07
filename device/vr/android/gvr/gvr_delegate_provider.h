// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_GVR_DELEGATE_PROVIDER_H_
#define DEVICE_VR_ANDROID_GVR_GVR_DELEGATE_PROVIDER_H_

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

  GvrDelegateProvider(const GvrDelegateProvider&) = delete;
  GvrDelegateProvider& operator=(const GvrDelegateProvider&) = delete;

  virtual bool ShouldDisableGvrDevice() = 0;
  virtual void StartWebXRPresentation(
      mojom::XRRuntimeSessionOptionsPtr options,
      base::OnceCallback<void(device::mojom::XRSessionPtr)> callback) = 0;
  virtual void ExitWebVRPresent() = 0;

 protected:
  virtual ~GvrDelegateProvider() = default;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_GVR_DELEGATE_PROVIDER_H_
