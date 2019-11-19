// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_manager_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/serial/serial_device_enumerator.h"
#include "services/device/serial/serial_port_impl.h"

namespace device {

SerialPortManagerImpl::SerialPortManagerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : io_task_runner_(std::move(io_task_runner)),
      ui_task_runner_(std::move(ui_task_runner)) {}

SerialPortManagerImpl::~SerialPortManagerImpl() = default;

void SerialPortManagerImpl::Bind(
    mojo::PendingReceiver<mojom::SerialPortManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SerialPortManagerImpl::SetSerialEnumeratorForTesting(
    std::unique_ptr<SerialDeviceEnumerator> fake_enumerator) {
  DCHECK(fake_enumerator);
  enumerator_ = std::move(fake_enumerator);
}

void SerialPortManagerImpl::GetDevices(GetDevicesCallback callback) {
  if (!enumerator_)
    enumerator_ = SerialDeviceEnumerator::Create();
  std::move(callback).Run(enumerator_->GetDevices());
}

void SerialPortManagerImpl::GetPort(
    const base::UnguessableToken& token,
    mojo::PendingReceiver<mojom::SerialPort> receiver,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher) {
  if (!enumerator_)
    enumerator_ = SerialDeviceEnumerator::Create();
  base::Optional<base::FilePath> path = enumerator_->GetPathFromToken(token);
  if (path) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SerialPortImpl::Create, *path, std::move(receiver),
                       std::move(watcher), ui_task_runner_));
  }
}

}  // namespace device
