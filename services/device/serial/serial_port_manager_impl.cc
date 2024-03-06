// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_manager_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/serial/bluetooth_serial_device_enumerator.h"
#include "services/device/serial/bluetooth_serial_port_impl.h"
#include "services/device/serial/serial_device_enumerator.h"
#include "services/device/serial/serial_port_impl.h"

namespace device {

namespace {

void OnPortOpened(mojom::SerialPortManager::OpenPortCallback callback,
                  const scoped_refptr<base::TaskRunner>& task_runner,
                  mojo::PendingRemote<mojom::SerialPort> port) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), std::move(port)));
}

void FinishGetDevices(SerialPortManagerImpl::GetDevicesCallback callback,
                      std::vector<mojom::SerialPortInfoPtr> devices,
                      std::vector<mojom::SerialPortInfoPtr> bluetooth_devices) {
  devices.insert(devices.end(),
                 std::make_move_iterator(bluetooth_devices.begin()),
                 std::make_move_iterator(bluetooth_devices.end()));
  std::move(callback).Run(std::move(devices));
}

}  // namespace

SerialPortManagerImpl::SerialPortManagerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : io_task_runner_(std::move(io_task_runner)),
      ui_task_runner_(std::move(ui_task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SerialPortManagerImpl::~SerialPortManagerImpl() {
  // Intentionally do not check sequence. See class comment doc for more info.
}

void SerialPortManagerImpl::Bind(
    mojo::PendingReceiver<mojom::SerialPortManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SerialPortManagerImpl::SetSerialEnumeratorForTesting(
    std::unique_ptr<SerialDeviceEnumerator> fake_enumerator) {
  DCHECK(fake_enumerator);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enumerator_ = std::move(fake_enumerator);
  observed_enumerator_.AddObservation(enumerator_.get());
}

void SerialPortManagerImpl::SetBluetoothSerialEnumeratorForTesting(
    std::unique_ptr<BluetoothSerialDeviceEnumerator>
        fake_bluetooth_enumerator) {
  DCHECK(fake_bluetooth_enumerator);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bluetooth_enumerator_ = std::move(fake_bluetooth_enumerator);
  observed_enumerator_.AddObservation(bluetooth_enumerator_.get());
}

void SerialPortManagerImpl::SetClient(
    mojo::PendingRemote<mojom::SerialPortManagerClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clients_.Add(std::move(client));
}

void SerialPortManagerImpl::GetDevices(GetDevicesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enumerator_) {
    enumerator_ = SerialDeviceEnumerator::Create(ui_task_runner_);
    observed_enumerator_.AddObservation(enumerator_.get());
  }
  auto devices = enumerator_->GetDevices();
  if (!base::FeatureList::IsEnabled(
          features::kEnableBluetoothSerialPortProfileInSerialApi)) {
    std::move(callback).Run(std::move(devices));
    return;
  }
  if (!bluetooth_enumerator_) {
    bluetooth_enumerator_ =
        std::make_unique<BluetoothSerialDeviceEnumerator>(ui_task_runner_);
    observed_enumerator_.AddObservation(bluetooth_enumerator_.get());
  }
  bluetooth_enumerator_->GetDevicesAfterInitialEnumeration(base::BindOnce(
      &FinishGetDevices, std::move(callback), std::move(devices)));
}

void SerialPortManagerImpl::OpenPort(
    const base::UnguessableToken& token,
    bool use_alternate_path,
    device::mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    OpenPortCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enumerator_) {
    enumerator_ = SerialDeviceEnumerator::Create(ui_task_runner_);
    observed_enumerator_.AddObservation(enumerator_.get());
  }
  std::optional<base::FilePath> path =
      enumerator_->GetPathFromToken(token, use_alternate_path);
  if (path) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SerialPortImpl::Open, *path, std::move(options), std::move(client),
            std::move(watcher), ui_task_runner_,
            base::BindOnce(&OnPortOpened, std::move(callback),
                           base::SequencedTaskRunner::GetCurrentDefault())));
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kEnableBluetoothSerialPortProfileInSerialApi)) {
    if (!bluetooth_enumerator_) {
      bluetooth_enumerator_ =
          std::make_unique<BluetoothSerialDeviceEnumerator>(ui_task_runner_);
      observed_enumerator_.AddObservation(bluetooth_enumerator_.get());
    }
    std::optional<std::string> address =
        bluetooth_enumerator_->GetAddressFromToken(token);
    if (address) {
      const BluetoothUUID service_class_id =
          bluetooth_enumerator_->GetServiceClassIdFromToken(token);
      ui_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &SerialPortManagerImpl::OpenBluetoothSerialPortOnUI,
              weak_factory_.GetWeakPtr(), *address, service_class_id,
              std::move(options), std::move(client), std::move(watcher),
              base::BindOnce(&OnPortOpened, std::move(callback),
                             base::SequencedTaskRunner::GetCurrentDefault())));
      return;
    }
  }

  std::move(callback).Run(mojo::NullRemote());
}

void SerialPortManagerImpl::OpenBluetoothSerialPortOnUI(
    const std::string& address,
    const BluetoothUUID& service_class_id,
    mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    BluetoothSerialPortImpl::OpenCallback callback) {
  bluetooth_enumerator_->OpenPort(address, service_class_id, std::move(options),
                                  std::move(client), std::move(watcher),
                                  std::move(callback));
}

void SerialPortManagerImpl::OnPortAdded(const mojom::SerialPortInfo& port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_)
    client->OnPortAdded(port.Clone());
}

void SerialPortManagerImpl::OnPortRemoved(const mojom::SerialPortInfo& port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_)
    client->OnPortRemoved(port.Clone());
}

void SerialPortManagerImpl::OnPortConnectedStateChanged(
    const mojom::SerialPortInfo& port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client->OnPortConnectedStateChanged(port.Clone());
  }
}

}  // namespace device
