// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/bluetooth_serial_device_enumerator.h"

#include <string_view>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/bluetooth_serial_port_impl.h"

namespace device {

namespace {

mojom::SerialPortInfoPtr CreatePort(std::string_view device_address,
                                    std::u16string_view device_name,
                                    const BluetoothUUID& service_class_id,
                                    bool connected) {
  auto port = mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->path = base::FilePath::FromUTF8Unsafe(device_address);
  port->type = mojom::SerialPortType::BLUETOOTH_CLASSIC_RFCOMM;
  port->bluetooth_service_class_id = service_class_id;
  port->display_name = base::UTF16ToUTF8(device_name);
  port->connected = connected;
  return port;
}

}  // namespace

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
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;

  void OpenPort(const std::string& address,
                const BluetoothUUID& service_class_id,
                mojom::SerialConnectionOptionsPtr options,
                mojo::PendingRemote<mojom::SerialPortClient> client,
                mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
                BluetoothSerialPortImpl::OpenCallback callback);

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
      base::BindOnce(
          &BluetoothSerialDeviceEnumerator::OnInitialEnumerationComplete,
          enumerator_));
}

void BluetoothSerialDeviceEnumerator::AdapterHelper::DeviceAdded(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enumerator_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSerialDeviceEnumerator::DeviceAddedOrChanged,
                     enumerator_, device->GetAddress(),
                     device->GetNameForDisplay(), device->GetUUIDs(),
                     device->IsConnected()));
}

void BluetoothSerialDeviceEnumerator::AdapterHelper::DeviceRemoved(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enumerator_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothSerialDeviceEnumerator::DeviceRemoved,
                                enumerator_, device->GetAddress()));
}

void BluetoothSerialDeviceEnumerator::AdapterHelper::DeviceChanged(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  DeviceAdded(adapter, device);
}

void BluetoothSerialDeviceEnumerator::AdapterHelper::OpenPort(
    const std::string& address,
    const BluetoothUUID& service_class_id,
    mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    BluetoothSerialPortImpl::OpenCallback callback) {
  BluetoothSerialPortImpl::Open(adapter_, address, service_class_id,
                                std::move(options), std::move(client),
                                std::move(watcher), std::move(callback));
}

BluetoothSerialDeviceEnumerator::BluetoothSerialDeviceEnumerator(
    scoped_refptr<base::SingleThreadTaskRunner> adapter_runner) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kEnableBluetoothSerialPortProfileInSerialApi));

  helper_ = base::SequenceBound<AdapterHelper>(
      std::move(adapter_runner), weak_ptr_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

BluetoothSerialDeviceEnumerator::~BluetoothSerialDeviceEnumerator() {
  std::vector<mojom::SerialPortManager::GetDevicesCallback> pending_callbacks;
  std::swap(pending_callbacks, pending_get_devices_);
  for (auto& callback : pending_callbacks) {
    std::move(callback).Run({});
  }
  CHECK(pending_get_devices_.empty());
}

void BluetoothSerialDeviceEnumerator::GetDevicesAfterInitialEnumeration(
    mojom::SerialPortManager::GetDevicesCallback callback) {
  if (initial_enumeration_completed_) {
    std::move(callback).Run(GetDevices());
    return;
  }
  pending_get_devices_.push_back(std::move(callback));
}

void BluetoothSerialDeviceEnumerator::DeviceAddedOrChanged(
    std::string_view device_address,
    std::u16string_view device_name,
    BluetoothDevice::UUIDSet service_class_ids,
    bool is_connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& service_class_id : service_class_ids) {
    AddOrUpdateService(device_address, device_name, service_class_id,
                       is_connected);
  }
}

void BluetoothSerialDeviceEnumerator::AddOrUpdateService(
    std::string_view device_address,
    std::u16string_view device_name,
    const BluetoothUUID& service_class_id,
    bool is_connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DeviceServiceInfo key =
      std::make_pair(std::string(device_address), service_class_id);
  auto find_it = device_ports_.find(key);
  if (find_it != device_ports_.end()) {
    if (base::FeatureList::IsEnabled(features::kSerialPortConnected)) {
      UpdatePortConnectedState(find_it->second, is_connected);
    }
    return;
  }

  auto port =
      CreatePort(device_address, device_name, service_class_id, is_connected);

  device_ports_.insert(std::make_pair(std::move(key), port->token));
  AddPort(std::move(port));
}

void BluetoothSerialDeviceEnumerator::OnInitialEnumerationComplete() {
  initial_enumeration_completed_ = true;
  std::vector<mojom::SerialPortManager::GetDevicesCallback> pending_callbacks;
  std::swap(pending_callbacks, pending_get_devices_);
  auto devices = GetDevices();
  for (auto& callback : pending_callbacks) {
    std::move(callback).Run(mojo::Clone(devices));
  }
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

void BluetoothSerialDeviceEnumerator::OpenPort(
    const std::string& address,
    const BluetoothUUID& service_class_id,
    mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    BluetoothSerialPortImpl::OpenCallback callback) {
  helper_.AsyncCall(&AdapterHelper::OpenPort)
      .WithArgs(address, service_class_id, std::move(options),
                std::move(client), std::move(watcher), std::move(callback));
}

std::optional<std::string> BluetoothSerialDeviceEnumerator::GetAddressFromToken(
    const base::UnguessableToken& token) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& entry : device_ports_) {
    if (entry.second == token)
      return entry.first.first;
  }
  return std::nullopt;
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

void BluetoothSerialDeviceEnumerator::DeviceAddedForTesting(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  // Pass the device to our helper, which will in turn pass it back to me.
  helper_.AsyncCall(&AdapterHelper::DeviceAdded)
      .WithArgs(base::Unretained(adapter), device);
}

void BluetoothSerialDeviceEnumerator::DeviceChangedForTesting(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  // Pass the device to our helper, which will in turn pass it back to me.
  helper_.AsyncCall(&AdapterHelper::DeviceChanged)
      .WithArgs(base::Unretained(adapter), device);
}

void BluetoothSerialDeviceEnumerator::SynchronouslyResetHelperForTesting() {
  helper_.SynchronouslyResetForTest();  // IN-TEST
}

}  // namespace device
