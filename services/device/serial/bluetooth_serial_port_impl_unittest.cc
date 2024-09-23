// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/serial/bluetooth_serial_port_impl.h"

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
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
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/test/fake_serial_port_client.h"
#include "services/device/public/mojom/serial.mojom.h"
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
const size_t kBufferNumBytes = std::char_traits<char>::length(kBuffer);
constexpr char kDiscardedBuffer[] = "discarded";
constexpr char kDeviceAddress[] = "00:00:00:00:00:00";
constexpr uint32_t kElementNumBytes = 1;
constexpr uint32_t kCapacityNumBytes = 64;

std::string CreateTestData(size_t buffer_size) {
  std::string test_data(buffer_size, 'X');
  for (size_t i = 0; i < test_data.size(); i++)
    test_data[i] = 1 + (i % 127);
  return test_data;
}

// Read all readable data from |consumer| into |read_data|.
MojoResult ReadConsumerData(mojo::ScopedDataPipeConsumerHandle& consumer,
                            std::string* read_data) {
  base::RunLoop run_loop;
  mojo::SimpleWatcher consumer_watcher(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
  MojoResult result = consumer_watcher.Watch(
      consumer.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindLambdaForTesting(
          [&](MojoResult watch_result, const mojo::HandleSignalsState& state) {
            EXPECT_EQ(watch_result, MOJO_RESULT_OK);
            if (watch_result != MOJO_RESULT_OK) {
              result = watch_result;
              run_loop.Quit();
              return;
            }
            if (state.readable()) {
              std::string read_buffer(32, '\0');
              size_t actually_read_bytes = 0;
              result =
                  consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                     base::as_writable_byte_span(read_buffer),
                                     actually_read_bytes);
              EXPECT_EQ(MOJO_RESULT_OK, result);
              if (result != MOJO_RESULT_OK) {
                run_loop.Quit();
                return;
              }
              read_data->append(
                  std::string_view(read_buffer).substr(0, actually_read_bytes));
            }
            if (state.peer_closed())
              run_loop.Quit();
          }));
  if (result != MOJO_RESULT_OK)
    return result;
  run_loop.Run();
  return result;
}

class BluetoothSerialPortImplTest : public testing::Test {
 public:
  BluetoothSerialPortImplTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kEnableBluetoothSerialPortProfileInSerialApi}, {});
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
        std::move(adapter), kDeviceAddress, GetSerialPortProfileUUID(),
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
    constexpr MojoCreateDataPipeOptions options = {
        .struct_size = sizeof(MojoCreateDataPipeOptions),
        .flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE,
        .element_num_bytes = kElementNumBytes,
        .capacity_num_bytes = kCapacityNumBytes,
    };

    MojoResult result = mojo::CreateDataPipe(&options, *producer, *consumer);
    DCHECK_EQ(result, MOJO_RESULT_OK);
  }

  MockBluetoothSocket& mock_socket() { return *mock_socket_; }

 private:
  scoped_refptr<MockBluetoothSocket> mock_socket_ =
      base::MakeRefCounted<MockBluetoothSocket>();
  std::unique_ptr<MockBluetoothDevice> mock_device_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
      std::move(adapter), kDeviceAddress, GetSerialPortProfileUUID(),
      mojom::SerialConnectionOptions::New(), FakeSerialPortClient::Create(),
      mojo::NullRemote(),
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

  size_t bytes_read = std::char_traits<char>::length(kBuffer);
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kBuffer);

  size_t actually_written_bytes = 0;
  MojoResult result =
      producer->WriteData(base::byte_span_from_cstring(kBuffer),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
  EXPECT_EQ(result, MOJO_RESULT_OK);

  EXPECT_CALL(mock_socket(), Send)
      .WillOnce(WithArgs<0, 1, 2>(Invoke(
          [&](scoped_refptr<net::IOBuffer> buf, int buffer_size,
              MockBluetoothSocket::SendCompletionCallback success_callback) {
            ASSERT_EQ(buffer_size, static_cast<int>(bytes_read));
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

  EXPECT_EQ(write_buffer->size(), static_cast<int>(bytes_read));

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

  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kBuffer);

  EXPECT_CALL(mock_socket(), Receive(_, _, _))
      .WillOnce(RunOnceCallback<1>(write_buffer->size(), write_buffer))
      .WillOnce(RunOnceCallback<2>(BluetoothSocket::kSystemError, "Error"));
  EXPECT_CALL(mock_socket(), Disconnect(_)).WillOnce(RunOnceCallback<0>());

  serial_port->StartReading(std::move(producer));

  std::string consumer_data;
  EXPECT_EQ(MOJO_RESULT_OK, ReadConsumerData(consumer, &consumer_data));
  ASSERT_EQ(kBufferNumBytes, consumer_data.size());
  for (size_t i = 0; i < consumer_data.size(); i++) {
    EXPECT_EQ(consumer_data[i], kBuffer[i])
        << "buffer comparison failed at index " << i;
  }

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(disconnect_loop.QuitClosure());

  serial_port.reset();
  disconnect_loop.Run();
}

TEST_F(BluetoothSerialPortImplTest, StartReadingLargeBufferTest) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  constexpr uint32_t kTestBufferNumBytes = 2 * kCapacityNumBytes;
  static_assert(kTestBufferNumBytes > kCapacityNumBytes,
                "must be greater than pipe capacity to test large reads.");
  const std::string test_data = CreateTestData(kTestBufferNumBytes);
  auto data_buffer = base::MakeRefCounted<net::StringIOBuffer>(test_data);

  std::string consumer_data;
  size_t total_bytes_read = 0;

  base::RunLoop watcher_loop;
  mojo::SimpleWatcher consumer_watcher(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
  MojoResult result = consumer_watcher.Watch(
      consumer.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindLambdaForTesting(
          [&](MojoResult result, const mojo::HandleSignalsState& state) {
            EXPECT_EQ(result, MOJO_RESULT_OK);
            if (state.readable()) {
              std::string read_buffer(32, '\0');
              size_t actually_read_bytes = 0;
              result =
                  consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                     base::as_writable_byte_span(read_buffer),
                                     actually_read_bytes);
              if (result == MOJO_RESULT_OK) {
                consumer_data.append(
                    read_buffer.substr(0, actually_read_bytes));
                total_bytes_read += actually_read_bytes;
              }
            } else if (state.peer_closed()) {
              watcher_loop.Quit();
            }
          }));
  EXPECT_EQ(MOJO_RESULT_OK, result);
  consumer_watcher.ArmOrNotify();

  // BluetoothSerialPortImpl::StartReading() will request to receive the
  // datapipe capacity (kCapacityNumBytes), but this test will respond with
  // a larger buffer size.
  EXPECT_CALL(mock_socket(), Receive(_, _, _))
      .WillOnce(RunOnceCallback<1>(data_buffer->size(), data_buffer))
      .WillOnce(RunOnceCallback<2>(BluetoothSocket::kSystemError, "Error"));

  EXPECT_CALL(mock_socket(), Disconnect(_)).WillOnce(RunOnceCallback<0>());

  serial_port->StartReading(std::move(producer));

  watcher_loop.Run();

  // Validate the data that was read is correct.
  ASSERT_EQ(total_bytes_read, kTestBufferNumBytes);
  for (size_t i = 0; i < total_bytes_read; i++) {
    EXPECT_EQ(test_data[i], consumer_data[i])
        << "consumer data invalid at index " << i;
  }

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(disconnect_loop.QuitClosure());

  serial_port.reset();
  disconnect_loop.Run();
}

TEST_F(BluetoothSerialPortImplTest, FlushWrite) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  EXPECT_CALL(mock_socket(), Send).Times(0);
  serial_port->StartWriting(std::move(consumer));

  // Calling Flush(kTransmit) should cause the data pipe to close.
  base::RunLoop peer_closed_loop;
  mojo::SimpleWatcher pipe_watcher(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
  MojoResult result = pipe_watcher.Watch(
      producer.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindLambdaForTesting(
          [&](MojoResult result, const mojo::HandleSignalsState& state) {
            EXPECT_EQ(result, MOJO_RESULT_OK);
            EXPECT_TRUE(state.peer_closed());
            peer_closed_loop.Quit();
          }));
  EXPECT_EQ(MOJO_RESULT_OK, result);

  base::RunLoop flush_loop;
  serial_port->Flush(mojom::SerialPortFlushMode::kTransmit,
                     flush_loop.QuitClosure());
  peer_closed_loop.Run();
  flush_loop.Run();

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(disconnect_loop.QuitClosure());

  serial_port.reset();
  disconnect_loop.Run();
}

TEST_F(BluetoothSerialPortImplTest, FlushWriteWithDataInPipe) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  size_t actually_written_bytes = 0;
  MojoResult result =
      producer->WriteData(base::byte_span_from_cstring(kBuffer),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
  EXPECT_EQ(result, MOJO_RESULT_OK);
  EXPECT_EQ(actually_written_bytes, std::char_traits<char>::length(kBuffer));

  EXPECT_CALL(mock_socket(), Send).Times(1);
  serial_port->StartWriting(std::move(consumer));

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

  base::RunLoop flush_loop;
  serial_port->Flush(mojom::SerialPortFlushMode::kTransmit,
                     flush_loop.QuitClosure());
  flush_loop.Run();
  watcher_loop.Run();

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(disconnect_loop.QuitClosure());

  serial_port.reset();
  disconnect_loop.Run();
}

TEST_F(BluetoothSerialPortImplTest, FlushWriteAndWriteNewPipe) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  // The data to be written to the serial port in this test.
  constexpr size_t kBufferSize = kCapacityNumBytes;
  constexpr size_t kBufferMidpointPos = kBufferSize / 2;
  const std::string write_data = CreateTestData(kBufferSize);
  size_t actually_written_bytes1 = 0xffffffff;
  MojoResult result;

  const std::string pre_flush_data =
      write_data.substr(/*pos=*/0, /*count=*/kBufferMidpointPos);

  const std::string post_flush_data = write_data.substr(
      kBufferMidpointPos, write_data.size() - kBufferMidpointPos);

  /*******************************************************************/
  /* Start writing a first time, which calls Send(...), but save the */
  /* completion callback so that it can be called after the Flush()  */
  /*******************************************************************/
  MockBluetoothSocket::SendCompletionCallback pre_flush_send_callback;
  {
    mojo::ScopedDataPipeProducerHandle pre_flush_producer;
    mojo::ScopedDataPipeConsumerHandle pre_flush_consumer;
    CreateDataPipe(&pre_flush_producer, &pre_flush_consumer);

    EXPECT_CALL(mock_socket(), Send)
        .WillOnce(WithArgs<0, 1, 2>(
            Invoke([&](scoped_refptr<net::IOBuffer> buf, int buffer_size,
                       MockBluetoothSocket::SendCompletionCallback callback) {
              EXPECT_EQ(buffer_size, static_cast<int>(actually_written_bytes1));
              DCHECK(!pre_flush_send_callback);
              for (int i = 0; i < buffer_size; i++) {
                EXPECT_EQ(buf->data()[i], pre_flush_data[i])
                    << "buffer comparison failed at index " << i;
              }
              pre_flush_send_callback = std::move(callback);
            })));

    result = pre_flush_producer->WriteData(base::as_byte_span(pre_flush_data),
                                           MOJO_WRITE_DATA_FLAG_NONE,
                                           actually_written_bytes1);
    EXPECT_EQ(result, MOJO_RESULT_OK);
    EXPECT_EQ(actually_written_bytes1, pre_flush_data.size());

    serial_port->StartWriting(std::move(pre_flush_consumer));

    // Calling Flush(kTransmit) causes the data pipe to close.
    base::RunLoop watcher_loop;
    mojo::SimpleWatcher pipe_watcher(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
    result = pipe_watcher.Watch(
        pre_flush_producer.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindLambdaForTesting(
            [&](MojoResult result, const mojo::HandleSignalsState& state) {
              EXPECT_EQ(result, MOJO_RESULT_OK);
              EXPECT_TRUE(state.peer_closed());
              watcher_loop.Quit();
            }));
    EXPECT_EQ(result, MOJO_RESULT_OK);

    base::RunLoop flush_loop;
    serial_port->Flush(mojom::SerialPortFlushMode::kTransmit,
                       flush_loop.QuitClosure());
    flush_loop.Run();
    watcher_loop.Run();
  }

  /***************************************************************************/
  /* Start writing a second time after flushing. Call the send callback for  */
  /* the first write. This simulates a pre flush in-flight Send() completing */
  /* after flushing and restarting writing.                                  */
  /***************************************************************************/
  mojo::ScopedDataPipeProducerHandle post_flush_producer;
  mojo::ScopedDataPipeConsumerHandle post_flush_consumer;
  CreateDataPipe(&post_flush_producer, &post_flush_consumer);

  size_t actually_written_bytes2 = 0;
  result = post_flush_producer->WriteData(base::as_byte_span(post_flush_data),
                                          MOJO_WRITE_DATA_FLAG_NONE,
                                          actually_written_bytes2);
  EXPECT_EQ(result, MOJO_RESULT_OK);
  EXPECT_EQ(actually_written_bytes2, post_flush_data.size());

  base::RunLoop post_flush_send_run_loop;

  EXPECT_CALL(mock_socket(), Send)
      .WillOnce(WithArgs<0, 1, 2>(
          Invoke([&](scoped_refptr<net::IOBuffer> buf, int buffer_size,
                     MockBluetoothSocket::SendCompletionCallback callback) {
            EXPECT_EQ(buffer_size, static_cast<int>(actually_written_bytes1));
            DCHECK(!pre_flush_send_callback);
            for (int i = 0; i < buffer_size; i++) {
              EXPECT_EQ(buf->data()[i], post_flush_data[i])
                  << "buffer comparison failed at index " << i;
            }
            std::move(callback).Run(buffer_size);
            post_flush_send_run_loop.Quit();
          })));

  serial_port->StartWriting(std::move(post_flush_consumer));
  // Wait for StartWriting to start on the remote end before directly calling
  // the receive callback - which executes on the remote end.
  serial_port.FlushForTesting();

  // Write the first half of the data to the pre-flush receive callback.
  ASSERT_TRUE(pre_flush_send_callback);
  std::move(pre_flush_send_callback).Run(pre_flush_data.size());

  post_flush_send_run_loop.Run();

  /************/
  /* Cleanup. */
  /************/
  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(disconnect_loop.QuitClosure());

  serial_port.reset();
  disconnect_loop.Run();
}

TEST_F(BluetoothSerialPortImplTest, FlushRead) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  const std::string test_data = CreateTestData(4);

  auto discarded_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kDiscardedBuffer);

  MockBluetoothSocket::ReceiveCompletionCallback pre_flush_receive_callback;
  EXPECT_CALL(mock_socket(), Receive)
      .WillOnce(
          [&](int buffer_size,
              MockBluetoothSocket::ReceiveCompletionCallback success_callback,
              MockBluetoothSocket::ReceiveErrorCompletionCallback
                  error_callback) {
            pre_flush_receive_callback = std::move(success_callback);
          });
  serial_port->StartReading(std::move(producer));

  // Calling Flush(kReceive) should cause the data pipe to close.
  {
    base::RunLoop watcher_loop;
    mojo::SimpleWatcher pipe_watcher(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
    MojoResult result = pipe_watcher.Watch(
        consumer.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindLambdaForTesting([&](MojoResult watcher_result,
                                       const mojo::HandleSignalsState& state) {
          EXPECT_EQ(watcher_result, MOJO_RESULT_OK);
          EXPECT_TRUE(state.peer_closed());
          watcher_loop.Quit();
        }));
    EXPECT_EQ(MOJO_RESULT_OK, result);

    base::RunLoop flush_loop;
    serial_port->Flush(mojom::SerialPortFlushMode::kReceive,
                       flush_loop.QuitClosure());
    flush_loop.Run();
    watcher_loop.Run();
  }

  // Running the Receive callback before StartReading() is called should result
  // in this data being discarded.
  ASSERT_TRUE(pre_flush_receive_callback);
  std::move(pre_flush_receive_callback)
      .Run(discarded_buffer->size(), discarded_buffer);

  mojo::ScopedDataPipeProducerHandle new_producer;
  mojo::ScopedDataPipeConsumerHandle new_consumer;
  CreateDataPipe(&new_producer, &new_consumer);

  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(test_data);
  EXPECT_CALL(mock_socket(), Receive(_, _, _))
      .WillOnce(RunOnceCallback<1>(write_buffer->size(), write_buffer))
      .WillOnce(RunOnceCallback<2>(BluetoothSocket::kSystemError, "Error"));
  serial_port->StartReading(std::move(new_producer));

  std::string consumer_data;
  EXPECT_EQ(MOJO_RESULT_OK, ReadConsumerData(new_consumer, &consumer_data));
  ASSERT_EQ(test_data.size(), consumer_data.size());
  for (size_t i = 0; i < consumer_data.size(); i++) {
    EXPECT_EQ(consumer_data[i], test_data[i])
        << "buffer comparison failed at index " << i;
  }

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(disconnect_loop.QuitClosure());

  serial_port.reset();
  disconnect_loop.Run();
}

TEST_F(BluetoothSerialPortImplTest, FlushReadAndReadNewPipe) {
  mojo::Remote<mojom::SerialPort> serial_port;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher;
  CreatePort(&serial_port, &watcher);

  // The data to be written to the serial port.
  constexpr size_t kBufferSize = kCapacityNumBytes - 1;
  constexpr size_t kBufferMidpointPos = kBufferSize / 2;
  const std::string write_data = CreateTestData(kBufferSize);

  // First half of data.
  auto pre_flush_buffer = base::MakeRefCounted<net::StringIOBuffer>(
      write_data.substr(/*pos=*/0, /*count=*/kBufferMidpointPos));

  // Second half of data.
  const std::string post_flush_data =
      write_data.substr(kBufferMidpointPos, kBufferSize - kBufferMidpointPos);

  MockBluetoothSocket::ReceiveCompletionCallback pre_flush_receive_callback;

  // Calling Flush(kReceive) will cause the data pipe to close.
  {
    mojo::ScopedDataPipeProducerHandle pre_flush_producer;
    mojo::ScopedDataPipeConsumerHandle pre_flush_consumer;
    CreateDataPipe(&pre_flush_producer, &pre_flush_consumer);

    EXPECT_CALL(mock_socket(), Receive)
        .WillOnce(
            [&](int buffer_size,
                MockBluetoothSocket::ReceiveCompletionCallback success_callback,
                MockBluetoothSocket::ReceiveErrorCompletionCallback
                    error_callback) {
              EXPECT_FALSE(pre_flush_receive_callback);
              pre_flush_receive_callback = std::move(success_callback);
            });

    base::RunLoop watcher_loop;
    mojo::SimpleWatcher write_watcher(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
    MojoResult result = result = write_watcher.Watch(
        pre_flush_consumer.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindLambdaForTesting(
            [&watcher_loop](MojoResult result,
                            const mojo::HandleSignalsState& state) {
              EXPECT_EQ(result, MOJO_RESULT_OK);
              EXPECT_TRUE(state.peer_closed());
              watcher_loop.Quit();
            }));
    EXPECT_EQ(MOJO_RESULT_OK, result);

    serial_port->StartReading(std::move(pre_flush_producer));

    base::RunLoop flush_loop;
    serial_port->Flush(mojom::SerialPortFlushMode::kReceive,
                       flush_loop.QuitClosure());
    flush_loop.Run();
    watcher_loop.Run();
    EXPECT_TRUE(pre_flush_receive_callback);
  }

  mojo::ScopedDataPipeProducerHandle post_flush_producer;
  mojo::ScopedDataPipeConsumerHandle post_flush_consumer;
  CreateDataPipe(&post_flush_producer, &post_flush_consumer);

  size_t num_write_bytes = 0;
  EXPECT_CALL(mock_socket(), Receive)
      .Times(2)
      .WillRepeatedly([&](int buffer_size,
                          MockBluetoothSocket::ReceiveCompletionCallback
                              success_callback,
                          MockBluetoothSocket::ReceiveErrorCompletionCallback
                              error_callback) {
        EXPECT_FALSE(pre_flush_receive_callback);
        if (!num_write_bytes) {
          num_write_bytes = post_flush_data.size();
          std::move(success_callback)
              .Run(post_flush_data.size(),
                   base::MakeRefCounted<net::StringIOBuffer>(post_flush_data));
        } else {
          std::move(error_callback).Run(BluetoothSocket::kSystemError, "Error");
        }
      });

  // Write the second half of the data after the flush.
  serial_port->StartReading(std::move(post_flush_producer));
  // Wait for StartReading to start on the remote end before directly calling
  // the receive callback - which executes on the remote end.
  serial_port.FlushForTesting();

  // Write the first half of the data to the pre-flush receive callback.
  ASSERT_TRUE(pre_flush_receive_callback);
  std::move(pre_flush_receive_callback)
      .Run(pre_flush_buffer->size(), pre_flush_buffer);

  std::string consumer_data;
  EXPECT_EQ(MOJO_RESULT_OK,
            ReadConsumerData(post_flush_consumer, &consumer_data));

  // Verify post flush receive data is received by the consumer in the correct
  // order.
  ASSERT_EQ(write_data.size(), consumer_data.size());
  for (size_t i = 0; i < consumer_data.size(); i++) {
    EXPECT_EQ(consumer_data[i], write_data[i])
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
  serial_port->Close(/*flush=*/false, close_loop.QuitClosure());
  close_loop.Run();

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(
      base::BindLambdaForTesting([&]() { disconnect_loop.Quit(); }));

  serial_port.reset();
  disconnect_loop.Run();
}

}  // namespace device
