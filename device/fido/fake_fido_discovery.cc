// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fake_fido_discovery.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace test {

// FakeFidoDiscovery ----------------------------------------------------------

FakeFidoDiscovery::FakeFidoDiscovery(FidoTransportProtocol transport,
                                     StartMode mode)
    : FidoDeviceDiscovery(transport), mode_(mode) {}

FakeFidoDiscovery::~FakeFidoDiscovery() = default;

void FakeFidoDiscovery::WaitForCallToStart() {
  wait_for_start_loop_.Run();
}

void FakeFidoDiscovery::SimulateStarted(bool success) {
  ASSERT_FALSE(is_running());
  NotifyDiscoveryStarted(success);
}

void FakeFidoDiscovery::WaitForCallToStartAndSimulateSuccess() {
  WaitForCallToStart();
  SimulateStarted(true /* success */);
}

void FakeFidoDiscovery::StartInternal() {
  wait_for_start_loop_.Quit();

  if (mode_ == StartMode::kAutomatic) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeFidoDiscovery::SimulateStarted,
                       weak_ptr_factory_.GetWeakPtr(), true /* success */));
  }
}

// FakeFidoDiscoveryFactory ---------------------------------------------

FakeFidoDiscoveryFactory::FakeFidoDiscoveryFactory() = default;
FakeFidoDiscoveryFactory::~FakeFidoDiscoveryFactory() = default;

FakeFidoDiscovery* FakeFidoDiscoveryFactory::ForgeNextHidDiscovery(
    FakeFidoDiscovery::StartMode mode) {
  next_hid_discovery_ = std::make_unique<FakeFidoDiscovery>(
      FidoTransportProtocol::kUsbHumanInterfaceDevice, mode);
  return next_hid_discovery_.get();
}

FakeFidoDiscovery* FakeFidoDiscoveryFactory::ForgeNextNfcDiscovery(
    FakeFidoDiscovery::StartMode mode) {
  next_nfc_discovery_ = std::make_unique<FakeFidoDiscovery>(
      FidoTransportProtocol::kNearFieldCommunication, mode);
  return next_nfc_discovery_.get();
}

FakeFidoDiscovery* FakeFidoDiscoveryFactory::ForgeNextCableDiscovery(
    FakeFidoDiscovery::StartMode mode) {
  next_cable_discovery_ =
      std::make_unique<FakeFidoDiscovery>(FidoTransportProtocol::kHybrid, mode);
  return next_cable_discovery_.get();
}

FakeFidoDiscovery* FakeFidoDiscoveryFactory::ForgeNextPlatformDiscovery(
    FakeFidoDiscovery::StartMode mode) {
  next_platform_discovery_list_.emplace_back(
      std::make_unique<FakeFidoDiscovery>(FidoTransportProtocol::kInternal,
                                          mode));
  return reinterpret_cast<FakeFidoDiscovery*>(
      next_platform_discovery_list_.back().get());
}

void FakeFidoDiscoveryFactory::set_discover_win_webauthn_api_authenticator(
    bool on) {
  discover_win_webauthn_api_authenticator_ = on;
}

std::vector<std::unique_ptr<FidoDiscoveryBase>>
FakeFidoDiscoveryFactory::Create(FidoTransportProtocol transport) {
  switch (transport) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return SingleDiscovery(std::move(next_hid_discovery_));
    case FidoTransportProtocol::kNearFieldCommunication:
      return SingleDiscovery(std::move(next_nfc_discovery_));
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return {};
    case FidoTransportProtocol::kHybrid:
      return SingleDiscovery(std::move(next_cable_discovery_));
    case FidoTransportProtocol::kInternal:
      return std::move(next_platform_discovery_list_);
    case FidoTransportProtocol::kDeprecatedAoa:
      break;
  }
  NOTREACHED();
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<device::FidoDiscoveryBase>
FakeFidoDiscoveryFactory::MaybeCreateWinWebAuthnApiDiscovery() {
  if (!discover_win_webauthn_api_authenticator_) {
    return nullptr;
  }

  return FidoDiscoveryFactory::MaybeCreateWinWebAuthnApiDiscovery();
}
#endif

}  // namespace test

}  // namespace device
