// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_impl.h"

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace device {

namespace {

class SerialPortImplTest : public DeviceServiceTestBase {
 public:
  SerialPortImplTest() = default;
  ~SerialPortImplTest() override = default;

 protected:
  DISALLOW_COPY_AND_ASSIGN(SerialPortImplTest);
};

TEST_F(SerialPortImplTest, WatcherClosedWhenPortClosed) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher;
  auto watcher_receiver = mojo::MakeSelfOwnedReceiver(
      std::make_unique<mojom::SerialPortConnectionWatcher>(),
      watcher.InitWithNewPipeAndPassReceiver());
  SerialPortImpl::Create(
      base::FilePath(), serial_port.BindNewPipeAndPassReceiver(),
      std::move(watcher), base::ThreadTaskRunnerHandle::Get());

  // To start with both the serial port connection and the connection watcher
  // connection should remain open.
  serial_port.FlushForTesting();
  EXPECT_TRUE(serial_port.is_connected());
  watcher_receiver->FlushForTesting();
  EXPECT_TRUE(watcher_receiver);

  // When the serial port connection is closed the watcher connection should be
  // closed.
  serial_port.reset();
  watcher_receiver->FlushForTesting();
  EXPECT_FALSE(watcher_receiver);
}

TEST_F(SerialPortImplTest, PortClosedWhenWatcherClosed) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher;
  auto watcher_receiver = mojo::MakeSelfOwnedReceiver(
      std::make_unique<mojom::SerialPortConnectionWatcher>(),
      watcher.InitWithNewPipeAndPassReceiver());
  SerialPortImpl::Create(
      base::FilePath(), serial_port.BindNewPipeAndPassReceiver(),
      std::move(watcher), base::ThreadTaskRunnerHandle::Get());

  // To start with both the serial port connection and the connection watcher
  // connection should remain open.
  serial_port.FlushForTesting();
  EXPECT_TRUE(serial_port.is_connected());
  watcher_receiver->FlushForTesting();
  EXPECT_TRUE(watcher_receiver);

  // When the watcher connection is closed, for safety, the serial port
  // connection should also be closed.
  watcher_receiver->Close();
  serial_port.FlushForTesting();
  EXPECT_FALSE(serial_port.is_connected());
}

}  // namespace

}  // namespace device
