// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SERIAL_PORT_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SERIAL_PORT_MANAGER_H_

#include <map>

#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace device {

class FakeSerialPortManager : public mojom::SerialPortManager {
 public:
  FakeSerialPortManager();

  FakeSerialPortManager(const FakeSerialPortManager&) = delete;
  FakeSerialPortManager& operator=(const FakeSerialPortManager&) = delete;

  ~FakeSerialPortManager() override;

  void AddReceiver(mojo::PendingReceiver<mojom::SerialPortManager> receiver);
  void AddPort(mojom::SerialPortInfoPtr port);
  void RemovePort(base::UnguessableToken token);

  void set_simulate_open_failure(bool simulate_open_failure) {
    simulate_open_failure_ = simulate_open_failure;
  }

  // mojom::SerialPortManager
  void SetClient(
      mojo::PendingRemote<mojom::SerialPortManagerClient> client) override;
  void GetDevices(GetDevicesCallback callback) override;
  void OpenPort(const base::UnguessableToken& token,
                bool use_alternate_path,
                device::mojom::SerialConnectionOptionsPtr options,
                mojo::PendingRemote<mojom::SerialPortClient> client,
                mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
                OpenPortCallback callback) override;

 private:
  std::map<base::UnguessableToken, mojom::SerialPortInfoPtr> ports_;
  mojo::ReceiverSet<mojom::SerialPortManager> receivers_;
  mojo::RemoteSet<mojom::SerialPortManagerClient> clients_;
  bool simulate_open_failure_ = false;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SERIAL_PORT_MANAGER_H_
