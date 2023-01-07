// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_impl.h"

#include "base/test/bind.h"
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

  void SimulateOpenFailure(bool fail) { fail_open_ = fail; }

  void SimulateGetControlSignalsFailure(bool fail) {
    fail_get_control_signals_ = fail;
  }

  void SimulateSetControlSignalsFailure(bool fail) {
    fail_set_control_signals_ = fail;
  }

  // SerialIoHandler implementation
  void Open(const mojom::SerialConnectionOptions& options,
            OpenCompleteCallback callback) override {
    std::move(callback).Run(!fail_open_);
  }

  void Flush(mojom::SerialPortFlushMode mode) const override {}
  void Drain() override {}

  mojom::SerialPortControlSignalsPtr GetControlSignals() const override {
    if (fail_get_control_signals_)
      return nullptr;

    return input_signals_.Clone();
  }

  bool SetControlSignals(
      const mojom::SerialHostControlSignals& control_signals) override {
    if (fail_set_control_signals_)
      return false;

    output_signals_ = control_signals;
    return true;
  }

  mojom::SerialConnectionInfoPtr GetPortInfo() const override {
    return mojom::SerialConnectionInfo::New();
  }

  void ReadImpl() override {}

  void WriteImpl() override {}

  void CancelReadImpl() override {
    ReadCompleted(/*bytes_read=*/0, mojom::SerialReceiveError::NONE);
  }

  void CancelWriteImpl() override {
    WriteCompleted(/*bytes_written=*/0, mojom::SerialSendError::NONE);
  }

  bool ConfigurePortImpl() override {
    // Open() is overridden so this should never be called.
    ADD_FAILURE() << "ConfigurePortImpl() should not be reached.";
    return false;
  }

 private:
  ~FakeSerialIoHandler() override = default;

  mojom::SerialPortControlSignals input_signals_;
  mojom::SerialHostControlSignals output_signals_;
  bool fail_open_ = false;
  bool fail_get_control_signals_ = false;
  bool fail_set_control_signals_ = false;
};

}  // namespace

class SerialPortImplTest : public DeviceServiceTestBase {
 public:
  SerialPortImplTest() = default;
  SerialPortImplTest(const SerialPortImplTest& other) = delete;
  void operator=(const SerialPortImplTest& other) = delete;
  ~SerialPortImplTest() override = default;

  scoped_refptr<FakeSerialIoHandler> CreatePort(
      mojo::Remote<mojom::SerialPort>* port,
      mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher>* watcher) {
    auto io_handler = base::MakeRefCounted<FakeSerialIoHandler>();
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher_remote;
    *watcher = mojo::MakeSelfOwnedReceiver(
        std::make_unique<mojom::SerialPortConnectionWatcher>(),
        watcher_remote.InitWithNewPipeAndPassReceiver());
    base::RunLoop loop;
    SerialPortImpl::OpenForTesting(
        io_handler, mojom::SerialConnectionOptions::New(), mojo::NullRemote(),
        std::move(watcher_remote),
        base::BindLambdaForTesting(
            [&](mojo::PendingRemote<mojom::SerialPort> pending_remote) {
              EXPECT_TRUE(pending_remote.is_valid());
              port->Bind(std::move(pending_remote));
              loop.Quit();
            }));
    loop.Run();
    return io_handler;
  }

  void CreateDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                      mojo::ScopedDataPipeConsumerHandle* consumer) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 64;

    MojoResult result = mojo::CreateDataPipe(&options, *producer, *consumer);
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

TEST_F(SerialPortImplTest, OpenFailure) {
  auto io_handler = base::MakeRefCounted<FakeSerialIoHandler>();
  io_handler->SimulateOpenFailure(true);

  mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<mojom::SerialPortConnectionWatcher>(),
      watcher_remote.InitWithNewPipeAndPassReceiver());
  base::RunLoop loop;
  SerialPortImpl::OpenForTesting(
      io_handler, mojom::SerialConnectionOptions::New(), mojo::NullRemote(),
      std::move(watcher_remote),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<mojom::SerialPort> pending_remote) {
            EXPECT_FALSE(pending_remote.is_valid());
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(SerialPortImplTest, GetControlSignalsFailure) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  scoped_refptr<FakeSerialIoHandler> io_handler =
      CreatePort(&serial_port, &watcher);
  io_handler->SimulateGetControlSignalsFailure(true);

  base::RunLoop loop;
  serial_port->GetControlSignals(base::BindLambdaForTesting(
      [&](mojom::SerialPortControlSignalsPtr signals) {
        EXPECT_FALSE(signals);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(SerialPortImplTest, SetControlSignalsFailure) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  scoped_refptr<FakeSerialIoHandler> io_handler =
      CreatePort(&serial_port, &watcher);
  io_handler->SimulateSetControlSignalsFailure(true);

  base::RunLoop loop;
  auto signals = mojom::SerialHostControlSignals::New();
  signals->has_dtr = true;
  signals->dtr = true;
  serial_port->SetControlSignals(std::move(signals),
                                 base::BindLambdaForTesting([&](bool success) {
                                   EXPECT_FALSE(success);
                                   loop.Quit();
                                 }));
  loop.Run();
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
  serial_port->Close(/*flush=*/true, loop.QuitClosure());
  loop.Run();
}

}  // namespace device
