// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_PORT_MANAGER_IMPL_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_PORT_MANAGER_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/bluetooth_serial_device_enumerator.h"
#include "services/device/serial/bluetooth_serial_port_impl.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace base {
class SingleThreadTaskRunner;
class UnguessableToken;
}  // namespace base

namespace device {

class BluetoothUUID;

// TODO(leonhsl): Merge this class with SerialDeviceEnumerator if/once
// SerialDeviceEnumerator is exposed only via the Device Service.
// crbug.com/748505
//
// Threading notes:
// 1. Created on the UI thread.
// 2. Used on the UI thread runner (macOS only), otherwise on a blocking task
//    runner.
// 3. Deleted on the same runner on which it is used *except* sometimes
//    during shutdown when the runner threadpool is already shutdown.
//    See crbug.com/1263149#c20 for details.
class SerialPortManagerImpl : public mojom::SerialPortManager,
                              public SerialDeviceEnumerator::Observer {
 public:
  SerialPortManagerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  SerialPortManagerImpl(const SerialPortManagerImpl&) = delete;
  SerialPortManagerImpl& operator=(const SerialPortManagerImpl&) = delete;

  ~SerialPortManagerImpl() override;

  void Bind(mojo::PendingReceiver<mojom::SerialPortManager> receiver);
  void SetSerialEnumeratorForTesting(
      std::unique_ptr<SerialDeviceEnumerator> fake_enumerator);
  void SetBluetoothSerialEnumeratorForTesting(
      std::unique_ptr<BluetoothSerialDeviceEnumerator>
          fake_bluetooth_enumerator);

 private:
  // mojom::SerialPortManager methods:
  void SetClient(
      mojo::PendingRemote<mojom::SerialPortManagerClient> client) override;
  void GetDevices(GetDevicesCallback callback) override;
  void OpenPort(const base::UnguessableToken& token,
                bool use_alternate_path,
                device::mojom::SerialConnectionOptionsPtr options,
                mojo::PendingRemote<mojom::SerialPortClient> client,
                mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
                OpenPortCallback callback) override;

  // SerialDeviceEnumerator::Observer methods:
  void OnPortAdded(const mojom::SerialPortInfo& port) override;
  void OnPortRemoved(const mojom::SerialPortInfo& port) override;
  void OnPortConnectedStateChanged(const mojom::SerialPortInfo& port) override;

  void OpenBluetoothSerialPortOnUI(
      const std::string& address,
      const BluetoothUUID& service_class_id,
      mojom::SerialConnectionOptionsPtr options,
      mojo::PendingRemote<mojom::SerialPortClient> client,
      mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
      BluetoothSerialPortImpl::OpenCallback callback);

  std::unique_ptr<SerialDeviceEnumerator> enumerator_;
  std::unique_ptr<BluetoothSerialDeviceEnumerator> bluetooth_enumerator_;
  base::ScopedMultiSourceObservation<SerialDeviceEnumerator,
                                     SerialDeviceEnumerator::Observer>
      observed_enumerator_{this};

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  mojo::ReceiverSet<SerialPortManager> receivers_;
  mojo::RemoteSet<mojom::SerialPortManagerClient> clients_;
  // See threading notes above for guidelines for checking sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SerialPortManagerImpl> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_PORT_MANAGER_IMPL_H_
