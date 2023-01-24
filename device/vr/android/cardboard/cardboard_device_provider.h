// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_PROVIDER_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_PROVIDER_H_

#include <memory>

#include "base/component_export.h"
#include "device/vr/public/cpp/vr_device_provider.h"

namespace device {
class CardboardDevice;

class COMPONENT_EXPORT(VR_CARDBOARD) CardboardDeviceProvider
    : public device::VRDeviceProvider {
 public:
  explicit CardboardDeviceProvider();
  ~CardboardDeviceProvider() override;

  CardboardDeviceProvider(const CardboardDeviceProvider&) = delete;
  CardboardDeviceProvider& operator=(const CardboardDeviceProvider&) = delete;

  void Initialize(device::VRDeviceProviderClient* client) override;
  bool Initialized() override;

 private:
  std::unique_ptr<device::CardboardDevice> cardboard_device_;
  bool initialized_ = false;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_PROVIDER_H_
