// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/bluetooth_serial_device_enumerator.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
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

  // scoped_refptr<BluetoothAdapter> is required to ensure that this object
  // actually has a reference to the BluetoothAdapter when the call to
  // RemoveObserver() happens.
  scoped_refptr<BluetoothAdapter> adapter_;

  // |observation_| needs to be after |adapter_| to ensure it is reset before
  // |adapter_|'s reset during destruction.
  base::ScopedObservation<BluetoothAdapter, BluetoothAdapter::Observer>
      observation_{this};
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(adapter);
  adapter_ = std::move(adapter);

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (auto* device : devices) {
    DeviceAdded(adapter_.get(), device);
  }
  observation_.Observe(adapter_.get());
  enumerator_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSerialDeviceEnumerator::SetClassicAdapter,
                     enumerator_, adapter_));
}

void BluetoothSerialDeviceEnumerator::AdapterHelper::DeviceAdded(
    BluetoothAdapter*,
    BluetoothDevice* device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enumerator_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSerialDeviceEnumerator::DeviceAdded, enumerator_,
                     device->GetAddress(), device->GetNameForDisplay(),
                     device->GetUUIDs()));
}

void BluetoothSerialDeviceEnumerator::AdapterHelper::DeviceRemoved(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enumerator_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothSerialDeviceEnumerator::DeviceRemoved,
                                enumerator_, device->GetAddress()));
}

BluetoothSerialDeviceEnumerator::BluetoothSerialDeviceEnumerator(
    scoped_refptr<base::SingleThreadTaskRunner> adapter_runner) {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBluetoothSerialPortProfileInSerialApi));

  helper_ = base::SequenceBound<AdapterHelper>(
      std::move(adapter_runner), weak_ptr_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

BluetoothSerialDeviceEnumerator::~BluetoothSerialDeviceEnumerator() = default;

void BluetoothSerialDeviceEnumerator::SetClassicAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(adapter);
  adapter_ = std::move(adapter);
  if (got_adapter_callback_) {
    std::move(got_adapter_callback_).Run();
  }
}

void BluetoothSerialDeviceEnumerator::DeviceAdded(
    base::StringPiece device_address,
    base::StringPiece16 device_name,
    BluetoothDevice::UUIDSet service_class_ids) {
  for (const auto& service_class_id : service_class_ids) {
    AddService(device_address, device_name, service_class_id);
  }
}

void BluetoothSerialDeviceEnumerator::AddService(
    base::StringPiece device_address,
    base::StringPiece16 device_name,
    const BluetoothUUID& service_class_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DeviceServiceInfo key =
      std::make_pair(std::string(device_address), service_class_id);
  if (base::Contains(device_ports_, key))
    return;

  auto port = mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->path = base::FilePath::FromUTF8Unsafe(device_address);
  port->type = mojom::DeviceType::SPP_DEVICE;
  // TODO(crbug.com/1261557): Use better name.
  // Using service class ID for development to disambiguate device services.
  const std::string device_name_utf8 = base::UTF16ToUTF8(device_name);
  if (service_class_id == GetSerialPortProfileUUID()) {
    port->display_name = device_name_utf8;
  } else {
    port->display_name = base::StringPrintf("%s [%s]", device_name_utf8.c_str(),
                                            service_class_id.value().c_str());
  }
  device_ports_.insert(std::make_pair(std::move(key), port->token));
  AddPort(std::move(port));
}

void BluetoothSerialDeviceEnumerator::DeviceRemoved(
    const std::string& device_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::EraseIf(device_ports_, [&](auto& map_entry) {
    if (map_entry.first.first == device_address) {
      RemovePort(/*token=*/map_entry.second);
      return true;
    }
    return false;
  });
}

scoped_refptr<BluetoothAdapter> BluetoothSerialDeviceEnumerator::GetAdapter() {
  return adapter_;
}

absl::optional<std::string>
BluetoothSerialDeviceEnumerator::GetAddressFromToken(
    const base::UnguessableToken& token) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& entry : device_ports_) {
    if (entry.second == token)
      return entry.first.first;
  }
  return absl::nullopt;
}

BluetoothUUID BluetoothSerialDeviceEnumerator::GetServiceClassIdFromToken(
    const base::UnguessableToken& token) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& entry : device_ports_) {
    if (entry.second == token)
      return entry.first.second;
  }
  return BluetoothUUID();
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
