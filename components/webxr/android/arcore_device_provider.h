// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_ARCORE_DEVICE_PROVIDER_H_
#define COMPONENTS_WEBXR_ANDROID_ARCORE_DEVICE_PROVIDER_H_

#include <memory>

#include "components/webxr/android/ar_compositor_delegate_provider.h"
#include "device/vr/public/cpp/vr_device_provider.h"

namespace device {

class ArCoreDevice;

}

namespace webxr {

class ArCoreDeviceProvider : public device::VRDeviceProvider {
 public:
  explicit ArCoreDeviceProvider(
      std::unique_ptr<webxr::ArCompositorDelegateProvider>
          compositor_delegate_provider);

  ArCoreDeviceProvider(const ArCoreDeviceProvider&) = delete;
  ArCoreDeviceProvider& operator=(const ArCoreDeviceProvider&) = delete;

  ~ArCoreDeviceProvider() override;
  void Initialize(device::VRDeviceProviderClient* client,
                  content::WebContents* initializing_web_contents) override;
  bool Initialized() override;

 private:
  std::unique_ptr<webxr::ArCompositorDelegateProvider>
      compositor_delegate_provider_;

  std::unique_ptr<device::ArCoreDevice> arcore_device_;
  bool initialized_ = false;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_ARCORE_DEVICE_PROVIDER_H_
