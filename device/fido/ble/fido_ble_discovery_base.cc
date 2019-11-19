// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_discovery_base.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/fido/ble/fido_ble_uuids.h"

namespace device {

FidoBleDiscoveryBase::FidoBleDiscoveryBase(FidoTransportProtocol transport)
    : FidoDeviceDiscovery(transport) {}

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
  FIDO_LOG(DEBUG) << "BLE discovery session started";
}

void FidoBleDiscoveryBase::OnSetPoweredError() {
  FIDO_LOG(ERROR) << "Failed to power on BLE adapter";
}

void FidoBleDiscoveryBase::OnStartDiscoverySessionError() {
  FIDO_LOG(ERROR) << "Failed to start BLE discovery";
}

void FidoBleDiscoveryBase::SetDiscoverySession(
    std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
  discovery_session_ = std::move(discovery_session);
}

bool FidoBleDiscoveryBase::IsCableDevice(const BluetoothDevice* device) const {
  const auto& uuid = CableAdvertisementUUID();
  return base::Contains(device->GetServiceData(), uuid) ||
         base::Contains(device->GetUUIDs(), uuid);
}

void FidoBleDiscoveryBase::OnGetAdapter(
    scoped_refptr<BluetoothAdapter> adapter) {
  if (!adapter->IsPresent()) {
    FIDO_LOG(DEBUG) << "No BLE adapter present";
    NotifyDiscoveryStarted(false);
    return;
  }

  DCHECK(!adapter_);
  adapter_ = std::move(adapter);
  DCHECK(adapter_);
  FIDO_LOG(DEBUG) << "BLE adapter address " << adapter_->GetAddress();

  adapter_->AddObserver(this);
  if (adapter_->IsPowered()) {
    OnSetPowered();
  }

  // FidoRequestHandlerBase blocks its transport availability callback on the
  // DiscoveryStarted() calls of all instantiated discoveries. Hence, this call
  // must not be put behind the BLE adapter getting powered on (which is
  // dependent on the UI), or else the UI and this discovery will wait on each
  // other indefinitely (see crbug.com/1018416).
  NotifyDiscoveryStarted(true);
}

void FidoBleDiscoveryBase::StartInternal() {
  BluetoothAdapterFactory::Get().GetAdapter(base::BindOnce(
      &FidoBleDiscoveryBase::OnGetAdapter, weak_factory_.GetWeakPtr()));
}

}  // namespace device
