// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_discovery.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "device/fido/ble/fido_ble_discovery.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_device_authenticator.h"

// HID is not supported on Android.
#if !defined(OS_ANDROID)
#include "device/fido/hid/fido_hid_discovery.h"
#endif  // !defined(OS_ANDROID)

namespace device {

namespace {

std::unique_ptr<FidoDeviceDiscovery> CreateFidoDiscoveryImpl(
    FidoTransportProtocol transport,
    service_manager::Connector* connector) {
  switch (transport) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
#if !defined(OS_ANDROID)
      DCHECK(connector);
      return std::make_unique<FidoHidDiscovery>(connector);
#else
      NOTREACHED() << "USB HID not supported on Android.";
      return nullptr;
#endif  // !defined(OS_ANDROID)
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return std::make_unique<FidoBleDiscovery>();
    case FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      NOTREACHED() << "Cable discovery is constructed using the dedicated "
                      "factory method.";
      return nullptr;
    case FidoTransportProtocol::kNearFieldCommunication:
      // TODO(https://crbug.com/825949): Add NFC support.
      return nullptr;
    case FidoTransportProtocol::kInternal:
      NOTREACHED() << "Internal authenticators should be handled separately.";
      return nullptr;
  }
  NOTREACHED() << "Unhandled transport type";
  return nullptr;
}

std::unique_ptr<FidoDeviceDiscovery> CreateCableDiscoveryImpl(
    std::vector<CableDiscoveryData> cable_data) {
  return std::make_unique<FidoCableDiscovery>(std::move(cable_data));
}

}  // namespace

FidoDeviceDiscovery::Observer::~Observer() = default;

// static
FidoDeviceDiscovery::FactoryFuncPtr FidoDeviceDiscovery::g_factory_func_ =
    &CreateFidoDiscoveryImpl;

// static
FidoDeviceDiscovery::CableFactoryFuncPtr
    FidoDeviceDiscovery::g_cable_factory_func_ = &CreateCableDiscoveryImpl;

// static
std::unique_ptr<FidoDeviceDiscovery> FidoDeviceDiscovery::Create(
    FidoTransportProtocol transport,
    service_manager::Connector* connector) {
  return (*g_factory_func_)(transport, connector);
}

//  static
std::unique_ptr<FidoDeviceDiscovery> FidoDeviceDiscovery::CreateCable(
    std::vector<CableDiscoveryData> cable_data) {
  return (*g_cable_factory_func_)(std::move(cable_data));
}

FidoDeviceDiscovery::FidoDeviceDiscovery(FidoTransportProtocol transport)
    : FidoDiscoveryBase(transport), weak_factory_(this) {}

FidoDeviceDiscovery::~FidoDeviceDiscovery() = default;

void FidoDeviceDiscovery::Start() {
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kStarting;

  // To ensure that that NotifiyStarted() is never invoked synchronously,
  // post task asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FidoDeviceDiscovery::StartInternal,
                                weak_factory_.GetWeakPtr()));
}

void FidoDeviceDiscovery::NotifyDiscoveryStarted(bool success) {
  DCHECK_EQ(state_, State::kStarting);
  if (success)
    state_ = State::kRunning;
  if (!observer())
    return;
  observer()->DiscoveryStarted(this, success);
}

void FidoDeviceDiscovery::NotifyAuthenticatorAdded(
    FidoAuthenticator* authenticator) {
  DCHECK_NE(state_, State::kIdle);
  if (!observer())
    return;
  observer()->AuthenticatorAdded(this, authenticator);
}

void FidoDeviceDiscovery::NotifyAuthenticatorRemoved(
    FidoAuthenticator* authenticator) {
  DCHECK_NE(state_, State::kIdle);
  if (!observer())
    return;
  observer()->AuthenticatorRemoved(this, authenticator);
}

std::vector<FidoDeviceAuthenticator*>
FidoDeviceDiscovery::GetAuthenticatorsForTesting() {
  std::vector<FidoDeviceAuthenticator*> authenticators;
  authenticators.reserve(authenticators_.size());
  for (const auto& authenticator : authenticators_)
    authenticators.push_back(authenticator.second.get());
  return authenticators;
}

std::vector<const FidoDeviceAuthenticator*>
FidoDeviceDiscovery::GetAuthenticatorsForTesting() const {
  std::vector<const FidoDeviceAuthenticator*> authenticators;
  authenticators.reserve(authenticators_.size());
  for (const auto& authenticator : authenticators_)
    authenticators.push_back(authenticator.second.get());
  return authenticators;
}

FidoDeviceAuthenticator* FidoDeviceDiscovery::GetAuthenticatorForTesting(
    base::StringPiece authenticator_id) {
  return GetAuthenticator(authenticator_id);
}

FidoDeviceAuthenticator* FidoDeviceDiscovery::GetAuthenticator(
    base::StringPiece authenticator_id) {
  auto found = authenticators_.find(authenticator_id);
  return found != authenticators_.end() ? found->second.get() : nullptr;
}

bool FidoDeviceDiscovery::AddDevice(std::unique_ptr<FidoDevice> device) {
  auto authenticator =
      std::make_unique<FidoDeviceAuthenticator>(std::move(device));
  const auto result =
      authenticators_.emplace(authenticator->GetId(), std::move(authenticator));
  if (!result.second) {
    return false;  // Duplicate device id.
  }

  NotifyAuthenticatorAdded(result.first->second.get());
  return true;
}

bool FidoDeviceDiscovery::RemoveDevice(base::StringPiece device_id) {
  auto found = authenticators_.find(device_id);
  if (found == authenticators_.end())
    return false;

  auto authenticator = std::move(found->second);
  authenticators_.erase(found);
  NotifyAuthenticatorRemoved(authenticator.get());
  return true;
}

// ScopedFidoDiscoveryFactory -------------------------------------------------

namespace internal {

ScopedFidoDiscoveryFactory::ScopedFidoDiscoveryFactory() {
  DCHECK(!g_current_factory);
  g_current_factory = this;
  original_factory_func_ =
      std::exchange(FidoDeviceDiscovery::g_factory_func_,
                    &ForwardCreateFidoDiscoveryToCurrentFactory);
  original_cable_factory_func_ =
      std::exchange(FidoDeviceDiscovery::g_cable_factory_func_,
                    &ForwardCreateCableDiscoveryToCurrentFactory);
}

ScopedFidoDiscoveryFactory::~ScopedFidoDiscoveryFactory() {
  g_current_factory = nullptr;
  FidoDeviceDiscovery::g_factory_func_ = original_factory_func_;
  FidoDeviceDiscovery::g_cable_factory_func_ = original_cable_factory_func_;
}

// static
std::unique_ptr<FidoDeviceDiscovery>
ScopedFidoDiscoveryFactory::ForwardCreateFidoDiscoveryToCurrentFactory(
    FidoTransportProtocol transport,
    ::service_manager::Connector* connector) {
  DCHECK(g_current_factory);
  return g_current_factory->CreateFidoDiscovery(transport, connector);
}

// static
std::unique_ptr<FidoDeviceDiscovery>
ScopedFidoDiscoveryFactory::ForwardCreateCableDiscoveryToCurrentFactory(
    std::vector<CableDiscoveryData> cable_data) {
  DCHECK(g_current_factory);
  g_current_factory->set_last_cable_data(std::move(cable_data));
  return g_current_factory->CreateFidoDiscovery(
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
      nullptr /* connector */);
}

// static
ScopedFidoDiscoveryFactory* ScopedFidoDiscoveryFactory::g_current_factory =
    nullptr;

}  // namespace internal
}  // namespace device
