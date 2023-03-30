// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_CARDBOARD_DEVICE_PROVIDER_H_
#define COMPONENTS_WEBXR_ANDROID_CARDBOARD_DEVICE_PROVIDER_H_

#include <memory>

#include "base/component_export.h"
#include "components/webxr/android/vr_compositor_delegate_provider.h"
#include "device/vr/public/cpp/vr_device_provider.h"

namespace device {
class CardboardDevice;
}
namespace webxr {

class CardboardDeviceProvider : public device::VRDeviceProvider {
 public:
  explicit CardboardDeviceProvider(
      std::unique_ptr<webxr::VrCompositorDelegateProvider>
          compositor_delegate_provider);
  ~CardboardDeviceProvider() override;

  CardboardDeviceProvider(const CardboardDeviceProvider&) = delete;
  CardboardDeviceProvider& operator=(const CardboardDeviceProvider&) = delete;

  void Initialize(device::VRDeviceProviderClient* client) override;
  bool Initialized() override;

 private:
  std::unique_ptr<device::CardboardDevice> cardboard_device_;
  std::unique_ptr<webxr::VrCompositorDelegateProvider>
      compositor_delegate_provider_;
  bool initialized_ = false;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_CARDBOARD_DEVICE_PROVIDER_H_
