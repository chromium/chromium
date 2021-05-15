// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/bluetooth_serial_device_enumerator.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/unguessable_token.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/cpp/serial/serial_switches.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace device {

BluetoothSerialDeviceEnumerator::BluetoothSerialDeviceEnumerator() {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBluetoothSerialPortProfileInSerialApi));
  device::BluetoothAdapterFactory::Get()->GetClassicAdapter(
      base::BindOnce(&BluetoothSerialDeviceEnumerator::OnGotClassicAdapter,
                     base::Unretained(this)));
}

BluetoothSerialDeviceEnumerator::~BluetoothSerialDeviceEnumerator() = default;

void BluetoothSerialDeviceEnumerator::OnGotClassicAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(adapter);
  adapter_ = adapter;
  adapter_->AddObserver(this);
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (auto* device : devices) {
    BluetoothDevice::UUIDSet device_uuids = device->GetUUIDs();
    if (base::Contains(device_uuids, GetSerialPortProfileUUID())) {
      auto port = mojom::SerialPortInfo::New();
      port->token = base::UnguessableToken::Create();
      port->path = base::FilePath::FromUTF8Unsafe(device->GetAddress());
      port->type = mojom::DeviceType::SPP_DEVICE;
      bluetooth_ports_.insert(
          std::make_pair(device->GetAddress(), port->token));
      AddPort(std::move(port));
    }
  }
}

void BluetoothSerialDeviceEnumerator::DeviceAdded(BluetoothAdapter* adapter,
                                                  BluetoothDevice* device) {
  BluetoothDevice::UUIDSet device_uuids = device->GetUUIDs();
  if (base::Contains(device_uuids, GetSerialPortProfileUUID())) {
    auto port = mojom::SerialPortInfo::New();
    port->token = base::UnguessableToken::Create();
    port->path = base::FilePath::FromUTF8Unsafe(device->GetAddress());
    port->type = mojom::DeviceType::SPP_DEVICE;
    bluetooth_ports_.insert(std::make_pair(device->GetAddress(), port->token));
    AddPort(std::move(port));
  }
}

void BluetoothSerialDeviceEnumerator::DeviceRemoved(BluetoothAdapter* adapter,
                                                    BluetoothDevice* device) {
  auto it = bluetooth_ports_.find(device->GetAddress());
  DCHECK(it != bluetooth_ports_.end());
  base::UnguessableToken token = it->second;
  bluetooth_ports_.erase(it);
  RemovePort(token);
}

scoped_refptr<BluetoothAdapter> BluetoothSerialDeviceEnumerator::GetAdapter() {
  return adapter_;
}

absl::optional<std::string>
BluetoothSerialDeviceEnumerator::GetAddressFromToken(
    const base::UnguessableToken& token) {
  for (const auto& entry : bluetooth_ports_) {
    if (entry.second == token)
      return entry.first;
  }
  return absl::nullopt;
}

}  // namespace device
