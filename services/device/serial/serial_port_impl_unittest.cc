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
  void CreateDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                      mojo::ScopedDataPipeConsumerHandle* consumer) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 64;

    MojoResult result = mojo::CreateDataPipe(&options, producer, consumer);
    DCHECK_EQ(result, MOJO_RESULT_OK);
  }

  mojo::ScopedDataPipeConsumerHandle StartReading(
      mojom::SerialPort* serial_port) {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    CreateDataPipe(&producer, &consumer);
    serial_port->StartReading(std::move(producer));
    return consumer;
  }

  mojo::ScopedDataPipeProducerHandle StartWriting(
      mojom::SerialPort* serial_port) {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    CreateDataPipe(&producer, &consumer);
    serial_port->StartWriting(std::move(consumer));
    return producer;
  }

  DISALLOW_COPY_AND_ASSIGN(SerialPortImplTest);
};

TEST_F(SerialPortImplTest, StartIoBeforeOpen) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher_remote;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher =
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<mojom::SerialPortConnectionWatcher>(),
          watcher_remote.InitWithNewPipeAndPassReceiver());
  SerialPortImpl::Create(
      base::FilePath(FILE_PATH_LITERAL("/dev/fakeserialmojo")),
      serial_port.BindNewPipeAndPassReceiver(), std::move(watcher_remote),
      task_environment_.GetMainThreadTaskRunner());

  mojo::ScopedDataPipeConsumerHandle consumer = StartReading(serial_port.get());
  mojo::ScopedDataPipeProducerHandle producer = StartWriting(serial_port.get());

  // Write some data so that StartWriting() will cause a call to Write().
  static const char kBuffer[] = "test";
  uint32_t bytes_written = base::size(kBuffer);
  MojoResult result =
      producer->WriteData(&kBuffer, &bytes_written, MOJO_WRITE_DATA_FLAG_NONE);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  DCHECK_EQ(bytes_written, base::size(kBuffer));

  base::RunLoop().RunUntilIdle();
}

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
