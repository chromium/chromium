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

// Helper class to interact with the BluetoothAdapter which must be accessed
// on a specific sequence.
class BluetoothSerialDeviceEnumerator::AdapterHelper
    : public BluetoothAdapter::Observer {
 public:
  AdapterHelper(base::WeakPtr<BluetoothSerialDeviceEnumerator> enumerator,
                scoped_refptr<base::SequencedTaskRunner> enumerator_runner);

  void OnGotClassicAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // BluetoothAdapter::Observer methods:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;

 private:
  // The enumerator that owns this instance.
  base::WeakPtr<BluetoothSerialDeviceEnumerator> enumerator_;
  scoped_refptr<base::SequencedTaskRunner> enumerator_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AdapterHelper> weak_ptr_factory_{this};
};

BluetoothSerialDeviceEnumerator::AdapterHelper::AdapterHelper(
    base::WeakPtr<BluetoothSerialDeviceEnumerator> enumerator,
    scoped_refptr<base::SequencedTaskRunner> enumerator_runner)
    : enumerator_(std::move(enumerator)),
      enumerator_runner_(std::move(enumerator_runner)) {
  device::BluetoothAdapterFactory::Get()->GetClassicAdapter(base::BindOnce(
      &BluetoothSerialDeviceEnumerator::AdapterHelper::OnGotClassicAdapter,
      weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothSerialDeviceEnumerator::AdapterHelper::OnGotClassicAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  SEQUENCE_CHECKER(sequence_checker_);
  DCHECK(adapter);
  adapter->AddObserver(this);
  std::vector<std::string> port_device_addresses;
  BluetoothAdapter::DeviceList devices = adapter->GetDevices();
  for (auto* device : devices) {
    BluetoothDevice::UUIDSet device_uuids = device->GetUUIDs();
    if (base::Contains(device_uuids, GetSerialPortProfileUUID())) {
      port_device_addresses.push_back(device->GetAddress());
    }
  }

  enumerator_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSerialDeviceEnumerator::SetClassicAdapter,
                     enumerator_, std::move(adapter),
                     std::move(port_device_addresses)));
}

void BluetoothSerialDeviceEnumerator::AdapterHelper::DeviceAdded(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BluetoothDevice::UUIDSet device_uuids = device->GetUUIDs();
  if (!base::Contains(device_uuids, GetSerialPortProfileUUID()))
    return;

  enumerator_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothSerialDeviceEnumerator::PortAdded,
                                enumerator_, device->GetAddress()));
}

void BluetoothSerialDeviceEnumerator::AdapterHelper::DeviceRemoved(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enumerator_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothSerialDeviceEnumerator::PortRemoved,
                                enumerator_, device->GetAddress()));
}

BluetoothSerialDeviceEnumerator::BluetoothSerialDeviceEnumerator(
    scoped_refptr<base::SingleThreadTaskRunner> adapter_runner) {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBluetoothSerialPortProfileInSerialApi));

  helper_ = base::SequenceBound<AdapterHelper>(
      std::move(adapter_runner), weak_ptr_factory_.GetWeakPtr(),
      base::SequencedTaskRunnerHandle::Get());
}

BluetoothSerialDeviceEnumerator::~BluetoothSerialDeviceEnumerator() = default;

void BluetoothSerialDeviceEnumerator::SetClassicAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter,
    std::vector<std::string> port_device_addresses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(adapter);
  adapter_ = std::move(adapter);
  DCHECK(device_ports_.empty());
  for (const auto& device_address : port_device_addresses) {
    PortAdded(device_address);
  }
  if (got_adapter_callback_) {
    std::move(got_adapter_callback_).Run();
  }
}

void BluetoothSerialDeviceEnumerator::PortAdded(
    const std::string& device_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(device_ports_, device_address));
  auto port = mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->path = base::FilePath::FromUTF8Unsafe(device_address);
  port->type = mojom::DeviceType::SPP_DEVICE;
  device_ports_.insert(std::make_pair(device_address, port->token));
  AddPort(std::move(port));
}

void BluetoothSerialDeviceEnumerator::PortRemoved(
    const std::string& device_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = device_ports_.find(device_address);
  DCHECK(it != device_ports_.end());
  base::UnguessableToken token = it->second;
  device_ports_.erase(it);
  RemovePort(token);
}

scoped_refptr<BluetoothAdapter> BluetoothSerialDeviceEnumerator::GetAdapter() {
  return adapter_;
}

absl::optional<std::string>
BluetoothSerialDeviceEnumerator::GetAddressFromToken(
    const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& entry : device_ports_) {
    if (entry.second == token)
      return entry.first;
  }
  return absl::nullopt;
}

void BluetoothSerialDeviceEnumerator::OnGotAdapterForTesting(
    base::OnceClosure closure) {
  if (adapter_) {
    std::move(closure).Run();
    return;
  }

  DCHECK(!got_adapter_callback_);
  got_adapter_callback_ = std::move(closure);
}

void BluetoothSerialDeviceEnumerator::DeviceAddedForTesting(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  // Pass the device to our helper, which will in turn pass it back to me.
  helper_.AsyncCall(&AdapterHelper::DeviceAdded)
      .WithArgs(base::Unretained(adapter), device);
}

void BluetoothSerialDeviceEnumerator::SynchronouslyResetHelperForTesting() {
  helper_.SynchronouslyResetForTest();  // IN-TEST
}

}  // namespace device
