// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_impl.h"

#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/serial_io_handler.h"

namespace device {

namespace {

class FakeSerialIoHandler : public SerialIoHandler {
 public:
  FakeSerialIoHandler()
      : SerialIoHandler(base::FilePath(), /*ui_thread_task_runner=*/nullptr) {}

  void Open(const mojom::SerialConnectionOptions& options,
            OpenCompleteCallback callback) override {
    std::move(callback).Run(true);
  }

  void Flush(mojom::SerialPortFlushMode mode) const override {}
  void Drain() override {}

  mojom::SerialPortControlSignalsPtr GetControlSignals() const override {
    return mojom::SerialPortControlSignals::New();
  }

  bool SetControlSignals(
      const mojom::SerialHostControlSignals& control_signals) override {
    return true;
  }

  mojom::SerialConnectionInfoPtr GetPortInfo() const override {
    return mojom::SerialConnectionInfo::New();
  }

  void ReadImpl() override {}

  void WriteImpl() override {}

  void CancelReadImpl() override {
    QueueReadCompleted(/*bytes_read=*/0, mojom::SerialReceiveError::NONE);
  }

  void CancelWriteImpl() override {
    QueueWriteCompleted(/*bytes_written=*/0, mojom::SerialSendError::NONE);
  }

  bool ConfigurePortImpl() override { return true; }

 private:
  ~FakeSerialIoHandler() override = default;
};

}  // namespace

class SerialPortImplTest : public DeviceServiceTestBase {
 public:
  SerialPortImplTest() = default;
  SerialPortImplTest(const SerialPortImplTest& other) = delete;
  void operator=(const SerialPortImplTest& other) = delete;
  ~SerialPortImplTest() override = default;

  void CreatePort(
      mojo::Remote<mojom::SerialPort>* port,
      mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher>* watcher) {
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher_remote;
    *watcher = mojo::MakeSelfOwnedReceiver(
        std::make_unique<mojom::SerialPortConnectionWatcher>(),
        watcher_remote.InitWithNewPipeAndPassReceiver());
    SerialPortImpl::CreateForTesting(
        base::MakeRefCounted<FakeSerialIoHandler>(),
        port->BindNewPipeAndPassReceiver(), std::move(watcher_remote));
  }

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
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  // To start with both the serial port connection and the connection watcher
  // connection should remain open.
  serial_port.FlushForTesting();
  EXPECT_TRUE(serial_port.is_connected());
  watcher->FlushForTesting();
  EXPECT_TRUE(watcher);

  // When the serial port connection is closed the watcher connection should be
  // closed.
  serial_port.reset();
  watcher->FlushForTesting();
  EXPECT_FALSE(watcher);
}

TEST_F(SerialPortImplTest, PortClosedWhenWatcherClosed) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  // To start with both the serial port connection and the connection watcher
  // connection should remain open.
  serial_port.FlushForTesting();
  EXPECT_TRUE(serial_port.is_connected());
  watcher->FlushForTesting();
  EXPECT_TRUE(watcher);

  // When the watcher connection is closed, for safety, the serial port
  // connection should also be closed.
  watcher->Close();
  serial_port.FlushForTesting();
  EXPECT_FALSE(serial_port.is_connected());
}

TEST_F(SerialPortImplTest, FlushRead) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeConsumerHandle consumer = StartReading(serial_port.get());

  // Calling Flush(kReceive) should cause the data pipe to close.
  base::RunLoop watcher_loop;
  mojo::SimpleWatcher pipe_watcher(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
  EXPECT_EQ(pipe_watcher.Watch(consumer.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                               MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                               base::BindLambdaForTesting(
                                   [&](MojoResult result,
                                       const mojo::HandleSignalsState& state) {
                                     EXPECT_EQ(result, MOJO_RESULT_OK);
                                     EXPECT_TRUE(state.peer_closed());
                                     watcher_loop.Quit();
                                   })),
            MOJO_RESULT_OK);

  base::RunLoop loop;
  serial_port->Flush(mojom::SerialPortFlushMode::kReceive, loop.QuitClosure());
  loop.Run();
  watcher_loop.Run();
}

TEST_F(SerialPortImplTest, FlushWrite) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeProducerHandle producer = StartWriting(serial_port.get());

  // Calling Flush(kTransmit) should cause the data pipe to close.
  base::RunLoop watcher_loop;
  mojo::SimpleWatcher pipe_watcher(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
  EXPECT_EQ(pipe_watcher.Watch(producer.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                               MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                               base::BindLambdaForTesting(
                                   [&](MojoResult result,
                                       const mojo::HandleSignalsState& state) {
                                     EXPECT_EQ(result, MOJO_RESULT_OK);
                                     EXPECT_TRUE(state.peer_closed());
                                     watcher_loop.Quit();
                                   })),
            MOJO_RESULT_OK);

  base::RunLoop loop;
  serial_port->Flush(mojom::SerialPortFlushMode::kTransmit, loop.QuitClosure());
  loop.Run();
  watcher_loop.Run();
}

TEST_F(SerialPortImplTest, Drain) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeProducerHandle producer = StartWriting(serial_port.get());

  // Drain() will wait for the data pipe to close before replying.
  producer.reset();

  base::RunLoop loop;
  serial_port->Drain(loop.QuitClosure());
  loop.Run();
}

TEST_F(SerialPortImplTest, Close) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  base::RunLoop loop;
  serial_port->Close(loop.QuitClosure());
  loop.Run();
}

}  // namespace device
