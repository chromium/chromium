// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/cardboard_device_provider.h"

#include "device/vr/android/cardboard/cardboard_device.h"
#include "device/vr/android/cardboard/cardboard_sdk_impl.h"

namespace webxr {

CardboardDeviceProvider::CardboardDeviceProvider() = default;
CardboardDeviceProvider::~CardboardDeviceProvider() = default;

void CardboardDeviceProvider::Initialize(
    device::VRDeviceProviderClient* client) {
  CHECK(!initialized_);
  DVLOG(2) << __func__ << ": Cardboard is supported, creating device";

  cardboard_device_ = std::make_unique<device::CardboardDevice>(
      std::make_unique<device::CardboardSdkImpl>());

  client->AddRuntime(cardboard_device_->GetId(),
                     cardboard_device_->GetDeviceData(),
                     cardboard_device_->BindXRRuntime());
  initialized_ = true;
  client->OnProviderInitialized();
}

bool CardboardDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace webxr
