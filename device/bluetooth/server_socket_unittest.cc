// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/server_socket.h"

#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/socket.h"
#include "device/bluetooth/test/fake_bluetooth_socket.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bluetooth {

class ServerSocketTest : public testing::Test {
 public:
  ServerSocketTest() = default;
  ~ServerSocketTest() override = default;
  ServerSocketTest(const ServerSocketTest&) = delete;
  ServerSocketTest& operator=(const ServerSocketTest&) = delete;

  void SetUp() override {
    mock_bluetooth_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    mock_bluetooth_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_bluetooth_adapter_.get(),
            /*class=*/0, "DeviceName", "DeviceAddress",
            /*paired=*/false,
            /*connected=*/false);

    fake_bluetooth_server_socket_ =
        base::MakeRefCounted<device::FakeBluetoothSocket>();
    server_socket_ =
        std::make_unique<ServerSocket>(fake_bluetooth_server_socket_);
  }

 protected:
  void Accept() {
    server_socket_->Accept(
        base::BindOnce(&ServerSocketTest::OnAccept, base::Unretained(this)));

    EXPECT_TRUE(fake_bluetooth_server_socket_->HasAcceptArgs());
  }

  std::unique_ptr<ServerSocket> server_socket_;
  scoped_refptr<device::FakeBluetoothSocket> fake_bluetooth_server_socket_;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
      mock_bluetooth_adapter_;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
      mock_bluetooth_device_;
  mojom::AcceptConnectionResultPtr accept_connection_result_;

 private:
  void OnAccept(mojom::AcceptConnectionResultPtr result) {
    accept_connection_result_ = std::move(result);
  }

  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(ServerSocketTest, TestOnDestroyCallsClose) {
  // When destroyed, |server_socket_| is expected to tear down its
  // BluetoothSocket.
  server_socket_.reset();
  EXPECT_TRUE(fake_bluetooth_server_socket_->called_disconnect());
}

TEST_F(ServerSocketTest, TestAccept_Success) {
  Accept();
  auto accept_args = fake_bluetooth_server_socket_->TakeAcceptArgs();

  auto success_callback = std::move(std::get<0>(*accept_args));
  auto fake_bluetooth_client_socket =
      base::MakeRefCounted<device::FakeBluetoothSocket>();
  std::move(success_callback)
      .Run(/*device=*/mock_bluetooth_device_.get(),
           /*bluetooth_socket=*/fake_bluetooth_client_socket);

  EXPECT_TRUE(accept_connection_result_);
  EXPECT_EQ(mock_bluetooth_device_->GetName(),
            accept_connection_result_->device->name);
  auto pending_socket = std::move(accept_connection_result_->socket);
  EXPECT_TRUE(pending_socket.is_valid());
  EXPECT_TRUE(accept_connection_result_->receive_stream.is_valid());
  EXPECT_TRUE(accept_connection_result_->send_stream.is_valid());

  // Ensure that the underlying BluetoothSocket is
  // |fake_bluetooth_client_socket|.
  mojo::Remote<mojom::Socket> socket(std::move(pending_socket));
  base::RunLoop run_loop;
  socket->Disconnect(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(fake_bluetooth_client_socket->called_disconnect());

  EXPECT_FALSE(fake_bluetooth_server_socket_->HasAcceptArgs());
}

TEST_F(ServerSocketTest, TestAccept_Error) {
  Accept();
  auto accept_args = fake_bluetooth_server_socket_->TakeAcceptArgs();

  auto error_callback = std::move(std::get<1>(*accept_args));
  std::move(error_callback).Run("ErrorMessage");

  EXPECT_FALSE(accept_connection_result_);

  EXPECT_FALSE(fake_bluetooth_server_socket_->HasAcceptArgs());
}

TEST_F(ServerSocketTest, TestDisconnect) {
  server_socket_->Disconnect(base::DoNothing());
  EXPECT_TRUE(fake_bluetooth_server_socket_->called_disconnect());
}

}  // namespace bluetooth
