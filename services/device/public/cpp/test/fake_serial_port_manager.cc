// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_serial_port_manager.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace device {

namespace {

class FakeSerialPort : public mojom::SerialPort {
 public:
  FakeSerialPort(
      mojo::PendingReceiver<mojom::SerialPort> receiver,
      mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher)
      : receiver_(this, std::move(receiver)), watcher_(std::move(watcher)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        [](FakeSerialPort* self) { delete self; }, base::Unretained(this)));
    watcher_.set_disconnect_handler(base::BindOnce(
        [](FakeSerialPort* self) { delete self; }, base::Unretained(this)));
  }

  ~FakeSerialPort() override = default;

  // mojom::SerialPort
  void Open(mojom::SerialConnectionOptionsPtr options,
            mojo::ScopedDataPipeConsumerHandle in_stream,
            mojo::ScopedDataPipeProducerHandle out_stream,
            mojo::PendingRemote<mojom::SerialPortClient> client,
            OpenCallback callback) override {
    in_stream_ = std::move(in_stream);
    out_stream_ = std::move(out_stream);
    client_.Bind(std::move(client));
    std::move(callback).Run(true);
  }

  void ClearSendError(mojo::ScopedDataPipeConsumerHandle consumer) override {
    NOTREACHED();
  }

  void ClearReadError(mojo::ScopedDataPipeProducerHandle producer) override {
    NOTREACHED();
  }

  void Flush(FlushCallback callback) override { NOTREACHED(); }

  void GetControlSignals(GetControlSignalsCallback callback) override {
    NOTREACHED();
  }

  void SetControlSignals(mojom::SerialHostControlSignalsPtr signals,
                         SetControlSignalsCallback callback) override {
    NOTREACHED();
  }

  void ConfigurePort(mojom::SerialConnectionOptionsPtr options,
                     ConfigurePortCallback callback) override {
    NOTREACHED();
  }

  void GetPortInfo(GetPortInfoCallback callback) override { NOTREACHED(); }

  void Close(CloseCallback callback) override { std::move(callback).Run(); }

 private:
  mojo::Receiver<mojom::SerialPort> receiver_;
  mojo::Remote<mojom::SerialPortConnectionWatcher> watcher_;

  // Mojo handles to keep open in order to simulate an active connection.
  mojo::ScopedDataPipeConsumerHandle in_stream_;
  mojo::ScopedDataPipeProducerHandle out_stream_;
  mojo::Remote<mojom::SerialPortClient> client_;

  DISALLOW_COPY_AND_ASSIGN(FakeSerialPort);
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
}

void FakeSerialPortManager::GetDevices(GetDevicesCallback callback) {
  std::vector<mojom::SerialPortInfoPtr> ports;
  for (const auto& map_entry : ports_)
    ports.push_back(map_entry.second.Clone());
  std::move(callback).Run(std::move(ports));
}

void FakeSerialPortManager::GetPort(
    const base::UnguessableToken& token,
    mojo::PendingReceiver<mojom::SerialPort> receiver,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher) {
  // The new FakeSerialPort instance is owned by the |receiver| and |watcher|
  // pipes.
  new FakeSerialPort(std::move(receiver), std::move(watcher));
}

}  // namespace device
