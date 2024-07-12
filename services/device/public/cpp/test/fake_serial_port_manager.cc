// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_serial_port_manager.h"

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/not_fatal_until.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace device {

namespace {

class FakeSerialPort : public mojom::SerialPort {
 public:
  FakeSerialPort(
      mojo::PendingRemote<mojom::SerialPortClient> client,
      mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher)
      : watcher_(std::move(watcher)), client_(std::move(client)) {
    watcher_.set_disconnect_handler(base::BindOnce(
        [](FakeSerialPort* self) { delete self; }, base::Unretained(this)));
  }

  FakeSerialPort(const FakeSerialPort&) = delete;
  FakeSerialPort& operator=(const FakeSerialPort&) = delete;

  ~FakeSerialPort() override = default;

  mojo::PendingRemote<mojom::SerialPort> BindNewPipeAndPassRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        [](FakeSerialPort* self) { delete self; }, base::Unretained(this)));
    return remote;
  }

  // mojom::SerialPort
  void StartWriting(mojo::ScopedDataPipeConsumerHandle consumer) override {
    in_stream_ = std::move(consumer);
  }

  void StartReading(mojo::ScopedDataPipeProducerHandle producer) override {
    out_stream_ = std::move(producer);
  }

  void Flush(device::mojom::SerialPortFlushMode mode,
             FlushCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void Drain(DrainCallback callback) override { NOTREACHED_IN_MIGRATION(); }

  void GetControlSignals(GetControlSignalsCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void SetControlSignals(mojom::SerialHostControlSignalsPtr signals,
                         SetControlSignalsCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void ConfigurePort(mojom::SerialConnectionOptionsPtr options,
                     ConfigurePortCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void GetPortInfo(GetPortInfoCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void Close(bool flush, CloseCallback callback) override {
    std::move(callback).Run();
  }

 private:
  mojo::Receiver<mojom::SerialPort> receiver_{this};
  mojo::Remote<mojom::SerialPortConnectionWatcher> watcher_;

  // Mojo handles to keep open in order to simulate an active connection.
  mojo::ScopedDataPipeConsumerHandle in_stream_;
  mojo::ScopedDataPipeProducerHandle out_stream_;
  mojo::Remote<mojom::SerialPortClient> client_;
};

}  // namespace

FakeSerialPortManager::FakeSerialPortManager() = default;

FakeSerialPortManager::~FakeSerialPortManager() = default;

void FakeSerialPortManager::AddReceiver(
    mojo::PendingReceiver<mojom::SerialPortManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FakeSerialPortManager::AddPort(mojom::SerialPortInfoPtr port) {
  base::UnguessableToken token = port->token;
  ports_[token] = std::move(port);

  for (auto& client : clients_)
    client->OnPortAdded(ports_[token]->Clone());
}

void FakeSerialPortManager::RemovePort(base::UnguessableToken token) {
  auto it = ports_.find(token);
  CHECK(it != ports_.end(), base::NotFatalUntil::M130);
  mojom::SerialPortInfoPtr info = std::move(it->second);
  ports_.erase(it);
  info->connected = false;

  for (auto& client : clients_)
    client->OnPortRemoved(info.Clone());
}

void FakeSerialPortManager::SetClient(
    mojo::PendingRemote<mojom::SerialPortManagerClient> client) {
  clients_.Add(std::move(client));
}

void FakeSerialPortManager::GetDevices(GetDevicesCallback callback) {
  std::vector<mojom::SerialPortInfoPtr> ports;
  for (const auto& map_entry : ports_)
    ports.push_back(map_entry.second.Clone());
  std::move(callback).Run(std::move(ports));
}

void FakeSerialPortManager::OpenPort(
    const base::UnguessableToken& token,
    bool use_alternate_path,
    device::mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    OpenPortCallback callback) {
  if (simulate_open_failure_) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  // This FakeSerialPort is owned by |receiver_| and |watcher_| and will
  // self-destruct on close.
  auto* port = new FakeSerialPort(std::move(client), std::move(watcher));
  std::move(callback).Run(port->BindNewPipeAndPassRemote());
}

}  // namespace device
