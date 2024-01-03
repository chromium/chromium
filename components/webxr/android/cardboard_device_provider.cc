// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/cardboard_device_provider.h"

#include "components/webxr/android/xr_activity_listener.h"
#include "components/webxr/android/xr_session_coordinator.h"
#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "device/vr/android/cardboard/cardboard_device.h"
#include "device/vr/android/cardboard/cardboard_sdk_impl.h"
#include "device/vr/android/cardboard/mock_cardboard_sdk.h"

namespace webxr {

// static
bool CardboardDeviceProvider::use_cardboard_mock_for_testing_ = false;

// static
void CardboardDeviceProvider::set_use_cardboard_mock_for_testing(bool value) {
  use_cardboard_mock_for_testing_ = value;
}

CardboardDeviceProvider::CardboardDeviceProvider(
    std::unique_ptr<webxr::VrCompositorDelegateProvider>
        compositor_delegate_provider)
    : compositor_delegate_provider_(std::move(compositor_delegate_provider)) {}

CardboardDeviceProvider::~CardboardDeviceProvider() = default;

void CardboardDeviceProvider::Initialize(
    device::VRDeviceProviderClient* client,
    content::WebContents* initializing_web_contents) {
  CHECK(!initialized_);
  DVLOG(2) << __func__ << ": Cardboard is supported, creating device";

  std::unique_ptr<device::CardboardSdk> sdk;
  if (use_cardboard_mock_for_testing_) {
    sdk = std::make_unique<device::MockCardboardSdk>();
  } else {
    sdk = std::make_unique<device::CardboardSdkImpl>();
  }
  CHECK(sdk);

  cardboard_device_ = std::make_unique<device::CardboardDevice>(
      std::move(sdk),
      std::make_unique<webxr::MailboxToSurfaceBridgeFactoryImpl>(),
      std::make_unique<webxr::XrSessionCoordinator>(),
      std::move(compositor_delegate_provider_),
      std::make_unique<webxr::XrActivityListenerFactory>());

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
