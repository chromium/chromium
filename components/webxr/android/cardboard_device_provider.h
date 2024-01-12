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
  static void set_use_cardboard_mock_for_testing(bool value);

  explicit CardboardDeviceProvider(
      std::unique_ptr<webxr::VrCompositorDelegateProvider>
          compositor_delegate_provider);
  ~CardboardDeviceProvider() override;

  CardboardDeviceProvider(const CardboardDeviceProvider&) = delete;
  CardboardDeviceProvider& operator=(const CardboardDeviceProvider&) = delete;

  void Initialize(device::VRDeviceProviderClient* client,
                  content::WebContents* initializing_web_contents) override;
  bool Initialized() override;

 private:
  // This flag forces to use the mock implementation of the
  // `device::CardboardSdk` interface. Meant to be used for testing purposes
  // only.
  static bool use_cardboard_mock_for_testing_;

  std::unique_ptr<device::CardboardDevice> cardboard_device_;
  std::unique_ptr<webxr::VrCompositorDelegateProvider>
      compositor_delegate_provider_;
  bool initialized_ = false;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_CARDBOARD_DEVICE_PROVIDER_H_
