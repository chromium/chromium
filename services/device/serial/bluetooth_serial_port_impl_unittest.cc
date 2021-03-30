// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/bluetooth_serial_port_impl.h"

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_socket.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/cpp/serial/serial_switches.h"
#include "services/device/public/cpp/test/fake_serial_port_client.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

constexpr char kBuffer[] = "test";
constexpr char kDeviceAddress[] = "00:00:00:00:00:00";
constexpr uint32_t kElementNumBytes = 1;
constexpr uint32_t kCapacityNumBytes = 64;

class BluetoothSerialPortImplTest : public testing::Test {
 public:
  BluetoothSerialPortImplTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableBluetoothSerialPortProfileInSerialApi);
  }
  BluetoothSerialPortImplTest(const BluetoothSerialPortImplTest&) = delete;
  BluetoothSerialPortImplTest& operator=(const BluetoothSerialPortImplTest&) =
      delete;
  ~BluetoothSerialPortImplTest() override = default;

  void CreatePort(
      mojo::Remote<mojom::SerialPort>* port,
      mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher>* watcher) {
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher_remote;
    *watcher = mojo::MakeSelfOwnedReceiver(
        std::make_unique<mojom::SerialPortConnectionWatcher>(),
        watcher_remote.InitWithNewPipeAndPassReceiver());

    scoped_refptr<MockBluetoothAdapter> adapter =
        base::MakeRefCounted<MockBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);
    mock_device_ = std::make_unique<MockBluetoothDevice>(
        adapter.get(), 0, "Test Device", kDeviceAddress, false, false);
    mock_device_->AddUUID(GetSerialPortProfileUUID());

    EXPECT_CALL(*adapter, GetDevice(kDeviceAddress))
        .WillOnce(Return(mock_device_.get()));
    EXPECT_CALL(*mock_device_,
                ConnectToService(GetSerialPortProfileUUID(), _, _))
        .WillOnce(RunOnceCallback<1>(mock_socket_));

    base::RunLoop loop;
    BluetoothSerialPortImpl::Open(
        std::move(adapter), kDeviceAddress,
        mojom::SerialConnectionOptions::New(), FakeSerialPortClient::Create(),
        std::move(watcher_remote),
        base::BindLambdaForTesting(
            [&](mojo::PendingRemote<mojom::SerialPort> remote) {
              EXPECT_TRUE(remote.is_valid());
              port->Bind(std::move(remote));
              loop.Quit();
            }));
    loop.Run();
  }

  void CreateDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                      mojo::ScopedDataPipeConsumerHandle* consumer) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = kElementNumBytes;
    options.capacity_num_bytes = kCapacityNumBytes;

    MojoResult result = mojo::CreateDataPipe(&options, *producer, *consumer);
    DCHECK_EQ(result, MOJO_RESULT_OK);
  }

  MockBluetoothSocket& mock_socket() { return *mock_socket_; }

 private:
  scoped_refptr<MockBluetoothSocket> mock_socket_ =
      base::MakeRefCounted<MockBluetoothSocket>();
  std::unique_ptr<MockBluetoothDevice> mock_device_;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

}  // namespace

TEST_F(BluetoothSerialPortImplTest, OpenFailure) {
  scoped_refptr<MockBluetoothAdapter> adapter =
      base::MakeRefCounted<MockBluetoothAdapter>();
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);
  auto mock_device = std::make_unique<MockBluetoothDevice>(
      adapter.get(), 0, "Test Device", kDeviceAddress, false, false);
  mock_device->AddUUID(GetSerialPortProfileUUID());

  EXPECT_CALL(*adapter, GetDevice(kDeviceAddress))
      .WillOnce(Return(mock_device.get()));
  EXPECT_CALL(*mock_device, ConnectToService(GetSerialPortProfileUUID(), _, _))
      .WillOnce(RunOnceCallback<2>("Error"));

  EXPECT_CALL(mock_socket(), Receive(_, _, _)).Times(0);
  EXPECT_CALL(mock_socket(), Disconnect(_)).Times(0);

  base::RunLoop loop;
  BluetoothSerialPortImpl::Open(
      std::move(adapter), kDeviceAddress, mojom::SerialConnectionOptions::New(),
      FakeSerialPortClient::Create(), mojo::NullRemote(),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<mojom::SerialPort> remote) {
            EXPECT_FALSE(remote.is_valid());
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(BluetoothSerialPortImplTest, StartWritingTest) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  uint32_t bytes_read = std::char_traits<char>::length(kBuffer);
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kBuffer);

  MojoResult result =
      producer->WriteData(&kBuffer, &bytes_read, MOJO_WRITE_DATA_FLAG_NONE);
  EXPECT_EQ(result, MOJO_RESULT_OK);

  EXPECT_CALL(mock_socket(), Send)
      .WillOnce(WithArgs<0, 1, 2>(Invoke(
          [&](scoped_refptr<net::IOBuffer> buf, int buffer_size,
              MockBluetoothSocket::SendCompletionCallback success_callback) {
            ASSERT_EQ(buffer_size, int{bytes_read});
            // EXPECT_EQ only does a shallow comparison, so it's necessary to
            // iterate through both objects and compare each character.
            for (int i = 0; i < buffer_size; i++) {
              EXPECT_EQ(buf->data()[i], kBuffer[i])
                  << "buffer comparison failed at index " << i;
            }
            std::move(success_callback).Run(buffer_size);
          })));

  EXPECT_CALL(mock_socket(), Disconnect(_)).WillOnce(RunOnceCallback<0>());

  serial_port->StartWriting(std::move(consumer));

  EXPECT_EQ(write_buffer->size(), int{bytes_read});

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(disconnect_loop.QuitClosure());

  serial_port.reset();
  disconnect_loop.Run();
}

TEST_F(BluetoothSerialPortImplTest, StartReadingTest) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  uint32_t bytes_read = std::char_traits<char>::length(kBuffer);
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kBuffer);

  MojoResult result =
      producer->WriteData(&kBuffer, &bytes_read, MOJO_WRITE_DATA_FLAG_NONE);
  EXPECT_EQ(result, MOJO_RESULT_OK);

  EXPECT_CALL(mock_socket(), Receive(_, _, _))
      .WillOnce(RunOnceCallback<1>(write_buffer->size(), write_buffer))
      .WillOnce(RunOnceCallback<2>(BluetoothSocket::kSystemError, "Error"));
  EXPECT_CALL(mock_socket(), Disconnect(_)).WillOnce(RunOnceCallback<0>());

  serial_port->StartReading(std::move(producer));

  ASSERT_EQ(write_buffer->size(), int{bytes_read});
  int size = write_buffer->size();
  // EXPECT_EQ only does a shallow comparison, so it's necessary to iterate
  // through both objects and compare each character.
  for (int i = 0; i < size; i++) {
    EXPECT_EQ(write_buffer->data()[i], kBuffer[i])
        << "buffer comparison failed at index " << i;
  }

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(disconnect_loop.QuitClosure());

  serial_port.reset();
  disconnect_loop.Run();
}

TEST_F(BluetoothSerialPortImplTest, Drain) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  serial_port->StartWriting(std::move(consumer));

  producer.reset();

  base::RunLoop drain_loop;
  serial_port->Drain(drain_loop.QuitClosure());
  drain_loop.Run();

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(disconnect_loop.QuitClosure());

  serial_port.reset();
  disconnect_loop.Run();
}

TEST_F(BluetoothSerialPortImplTest, Close) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  EXPECT_CALL(mock_socket(), Disconnect(_)).WillOnce(RunOnceCallback<0>());

  base::RunLoop close_loop;
  serial_port->Close(close_loop.QuitClosure());
  close_loop.Run();

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(
      base::BindLambdaForTesting([&]() { disconnect_loop.Quit(); }));

  serial_port.reset();
  disconnect_loop.Run();
}

}  // namespace device
