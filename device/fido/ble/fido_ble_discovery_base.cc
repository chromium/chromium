// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_discovery_base.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/fido/ble/fido_ble_uuids.h"

namespace device {

FidoBleDiscoveryBase::FidoBleDiscoveryBase(FidoTransportProtocol transport)
    : FidoDeviceDiscovery(transport), weak_factory_(this) {}

FidoBleDiscoveryBase::~FidoBleDiscoveryBase() {
  if (adapter_)
    adapter_->RemoveObserver(this);

  // Destroying |discovery_session_| will best-effort-stop discovering.
}

// static
const BluetoothUUID& FidoBleDiscoveryBase::CableAdvertisementUUID() {
  static const base::NoDestructor<BluetoothUUID> service_uuid(
      kCableAdvertisementUUID128);
  return *service_uuid;
}

void FidoBleDiscoveryBase::OnStartDiscoverySessionWithFilter(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  SetDiscoverySession(std::move(session));
  DVLOG(2) << "Discovery session started.";
  NotifyDiscoveryStarted(true);
}

void FidoBleDiscoveryBase::OnSetPoweredError() {
  DLOG(ERROR) << "Failed to power on the adapter.";
  NotifyDiscoveryStarted(false);
}

void FidoBleDiscoveryBase::OnStartDiscoverySessionError() {
  DLOG(ERROR) << "Discovery session not started.";
  NotifyDiscoveryStarted(false);
}

void FidoBleDiscoveryBase::SetDiscoverySession(
    std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
  discovery_session_ = std::move(discovery_session);
}

bool FidoBleDiscoveryBase::IsCableDevice(const BluetoothDevice* device) const {
  const auto& uuid = CableAdvertisementUUID();
  return base::ContainsKey(device->GetServiceData(), uuid) ||
         base::ContainsKey(device->GetUUIDs(), uuid);
}

void FidoBleDiscoveryBase::OnGetAdapter(
    scoped_refptr<BluetoothAdapter> adapter) {
  if (!adapter->IsPresent()) {
    DVLOG(2) << "bluetooth adapter is not available in current system.";
    NotifyDiscoveryStarted(false);
    return;
  }

  DCHECK(!adapter_);
  adapter_ = std::move(adapter);
  DCHECK(adapter_);
  DVLOG(2) << "Got adapter " << adapter_->GetAddress();

  adapter_->AddObserver(this);
  if (adapter_->IsPowered())
    OnSetPowered();
}

void FidoBleDiscoveryBase::StartInternal() {
  BluetoothAdapterFactory::Get().GetAdapter(
      base::AdaptCallbackForRepeating(base::BindOnce(
          &FidoBleDiscoveryBase::OnGetAdapter, weak_factory_.GetWeakPtr())));
}

}  // namespace device
