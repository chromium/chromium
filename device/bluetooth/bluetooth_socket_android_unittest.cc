// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_android.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_device_android.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/test/bluetooth_test_android.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

std::vector<uint8_t> GenerateMessage() {
  constexpr size_t kMessageSize = 8192;
  std::vector<uint8_t> message(kMessageSize);

  for (size_t i = 0; i < kMessageSize; ++i) {
    uint16_t value = i % 512;
    value = value < 256 ? value : 512 - value;
    message[i] = value;
  }

  return message;
}

}  // namespace

class BluetoothSocketAndroidTest
    : public BluetoothTestAndroid,
      public testing::WithParamInterface<
          base::RepeatingCallback<void(BluetoothSocketAndroidTest*)>> {
 public:
  // BluetoothTestAndroid overrides
  void SetUp() override;
  void TearDown() override;

  void Connect();
  void ConnectInsecurely();

 protected:
  void ConnectSocket();
  void DisconnectSocket();

  raw_ptr<BluetoothDeviceAndroid> device_;
  scoped_refptr<BluetoothSocket> socket_;

  std::string last_error_message_;
};

void BluetoothSocketAndroidTest::SetUp() {
  BluetoothTestAndroid::SetUp();

  InitWithFakeAdapter();

  device_ = static_cast<BluetoothDeviceAndroid*>(
      adapter_->GetDevice(SimulatePairedClassicDevice(1)));
}

void BluetoothSocketAndroidTest::TearDown() {
  if (socket_) {
    DisconnectSocket();
  }

  BluetoothTestAndroid::TearDown();
}

void BluetoothSocketAndroidTest::Connect() {
  base::RunLoop run_loop;
  device_->ConnectToService(
      BluetoothUUID(kTestUUIDSerial),
      base::BindLambdaForTesting([&](scoped_refptr<BluetoothSocket> socket) {
        socket_ = std::move(socket);
        run_loop.Quit();
      }),
      base::BindLambdaForTesting([&](const std::string& error_message) {
        last_error_message_ = error_message;
        run_loop.Quit();
      }));
  run_loop.Run();
}

void BluetoothSocketAndroidTest::ConnectInsecurely() {
  base::RunLoop run_loop;
  device_->ConnectToServiceInsecurely(
      BluetoothUUID(kTestUUIDSerial),
      base::BindLambdaForTesting([&](scoped_refptr<BluetoothSocket> socket) {
        socket_ = std::move(socket);
        run_loop.Quit();
      }),
      base::BindLambdaForTesting([&](const std::string& error_message) {
        last_error_message_ = error_message;
        run_loop.Quit();
      }));
  run_loop.Run();
}

void BluetoothSocketAndroidTest::ConnectSocket() {
  GetParam().Run(this);
}

void BluetoothSocketAndroidTest::DisconnectSocket() {
  base::RunLoop run_loop;
  socket_->Disconnect(base::BindLambdaForTesting([&] {
    socket_.reset();
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_FALSE(socket_);
}

TEST_P(BluetoothSocketAndroidTest, Connect) {
  ConnectSocket();

  EXPECT_TRUE(socket_);
  EXPECT_EQ(last_error_message_, std::string());
}

TEST_P(BluetoothSocketAndroidTest, ConnectFailed) {
  const std::string kErrorMessage = "Test error";
  FailNextServiceConnection(device_, kErrorMessage);

  ConnectSocket();

  EXPECT_FALSE(socket_);
  EXPECT_EQ(last_error_message_, kErrorMessage);
}

TEST_P(BluetoothSocketAndroidTest, DisconnectFailed) {
  ConnectSocket();

  FailNextOperation(socket_.get(), "Test error");

  DisconnectSocket();
}

TEST_P(BluetoothSocketAndroidTest, Receive) {
  ConnectSocket();

  std::vector<uint8_t> message = GenerateMessage();
  SetReceivedBytes(socket_.get(), message);

  std::vector<uint8_t> received_message;
  received_message.reserve(message.size());
  constexpr size_t kBufferSize = 2000;
  for (size_t i = 0; i < message.size() / kBufferSize; i++) {
    base::RunLoop run_loop;
    socket_->Receive(
        kBufferSize,
        base::BindLambdaForTesting([&](int bytes_received,
                                       scoped_refptr<net::IOBuffer> buffer) {
          ASSERT_EQ(static_cast<size_t>(bytes_received), kBufferSize);
          received_message.insert(received_message.end(),
                                  buffer->span().begin(), buffer->span().end());
          run_loop.Quit();
        }),
        base::BindLambdaForTesting(
            [&](device::BluetoothSocket::ErrorReason reason,
                const std::string& error_message) {
              last_error_message_ = error_message;
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  size_t remainder = message.size() % kBufferSize;
  base::RunLoop run_loop;
  socket_->Receive(
      kBufferSize,
      base::BindLambdaForTesting([&](int bytes_received,
                                     scoped_refptr<net::IOBuffer> buffer) {
        ASSERT_EQ(static_cast<size_t>(bytes_received), remainder);
        received_message.insert(received_message.end(), buffer->span().begin(),
                                buffer->span().begin() + remainder);
        run_loop.Quit();
      }),
      base::BindLambdaForTesting(
          [&](device::BluetoothSocket::ErrorReason reason,
              const std::string& error_message) {
            last_error_message_ = error_message;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_EQ(message, received_message);
  EXPECT_EQ(last_error_message_, std::string());
}

TEST_P(BluetoothSocketAndroidTest, ReceiveFailed) {
  ConnectSocket();

  const std::string kErrorMessage = "Test error";
  FailNextOperation(socket_.get(), kErrorMessage);

  constexpr size_t kBufferSize = 2000;
  base::RunLoop run_loop;
  socket_->Receive(
      kBufferSize,
      base::BindLambdaForTesting(
          [&](int bytes_received, scoped_refptr<net::IOBuffer> buffer) {
            ADD_FAILURE();
            run_loop.Quit();
          }),
      base::BindLambdaForTesting([&](BluetoothSocket::ErrorReason reason,
                                     const std::string& error_message) {
        EXPECT_EQ(reason, BluetoothSocket::ErrorReason::kSystemError);
        last_error_message_ = error_message;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(last_error_message_, kErrorMessage);
}

TEST_P(BluetoothSocketAndroidTest, ReceiveAfterDisconnect) {
  ConnectSocket();
  scoped_refptr<BluetoothSocket> socket = socket_;
  DisconnectSocket();

  constexpr size_t kBufferSize = 2000;
  base::RunLoop run_loop;
  socket->Receive(
      kBufferSize,
      base::BindLambdaForTesting(
          [&](int bytes_received, scoped_refptr<net::IOBuffer> buffer) {
            ADD_FAILURE();
            run_loop.Quit();
          }),
      base::BindLambdaForTesting([&](BluetoothSocket::ErrorReason reason,
                                     const std::string& error_message) {
        EXPECT_EQ(reason, BluetoothSocket::ErrorReason::kDisconnected);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_P(BluetoothSocketAndroidTest, Send) {
  ConnectSocket();

  std::vector<uint8_t> message = GenerateMessage();

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::VectorIOBuffer>(message);
  constexpr size_t kBufferSize = 2000;

  base::RunLoop run_loop;
  socket_->Send(
      buffer, kBufferSize, base::BindLambdaForTesting([&](int bytes_sent) {
        EXPECT_EQ(static_cast<size_t>(bytes_sent), kBufferSize);
        run_loop.Quit();
      }),
      base::BindLambdaForTesting([&](const std::string& error_message) {
        last_error_message_ = error_message;
        run_loop.Quit();
      }));
  run_loop.Run();

  std::vector<uint8_t> sent_message = GetSentBytes(socket_.get());
  message.resize(kBufferSize);
  EXPECT_EQ(message, sent_message);
  EXPECT_EQ(last_error_message_, std::string());
}

TEST_P(BluetoothSocketAndroidTest, SendFailed) {
  ConnectSocket();

  std::vector<uint8_t> message = GenerateMessage();

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::VectorIOBuffer>(message);
  constexpr size_t kBufferSize = 2000;

  std::string error_message = "Test error";
  FailNextOperation(socket_.get(), error_message);

  base::RunLoop run_loop;
  socket_->Send(
      buffer, kBufferSize, base::BindLambdaForTesting([&](int bytes_sent) {
        ADD_FAILURE();
        run_loop.Quit();
      }),
      base::BindLambdaForTesting([&](const std::string& error_message) {
        last_error_message_ = error_message;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(last_error_message_, error_message);
}

TEST_P(BluetoothSocketAndroidTest, SendAfterDisconnect) {
  ConnectSocket();
  scoped_refptr<BluetoothSocket> socket = socket_;
  DisconnectSocket();

  std::vector<uint8_t> message = GenerateMessage();

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::VectorIOBuffer>(message);
  constexpr size_t kBufferSize = 2000;

  base::RunLoop run_loop;
  socket->Send(
      buffer, kBufferSize, base::BindLambdaForTesting([&](int bytes_sent) {
        ADD_FAILURE();
        run_loop.Quit();
      }),
      base::BindLambdaForTesting([&](const std::string& error_message) {
        last_error_message_ = error_message;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_NE(last_error_message_, std::string());
}

INSTANTIATE_TEST_SUITE_P(
    BluetoothSocketAndroidTests,
    BluetoothSocketAndroidTest,
    testing::Values(
        base::BindRepeating(&BluetoothSocketAndroidTest::Connect),
        base::BindRepeating(&BluetoothSocketAndroidTest::ConnectInsecurely)));

}  // namespace device
