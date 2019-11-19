// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_socket_bluez.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_service_provider.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothSocket;
using device::BluetoothSocketThread;
using device::BluetoothUUID;

namespace bluez {

class BluetoothSocketBlueZTest : public testing::Test {
 public:
  BluetoothSocketBlueZTest()
      : success_callback_count_(0),
        error_callback_count_(0),
        last_bytes_sent_(0),
        last_bytes_received_(0),
        last_reason_(BluetoothSocket::kSystemError) {}

  void SetUp() override {
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();

    dbus_setter->SetBluetoothAdapterClient(
        std::unique_ptr<bluez::BluetoothAdapterClient>(
            new bluez::FakeBluetoothAdapterClient));
    dbus_setter->SetBluetoothAgentManagerClient(
        std::unique_ptr<bluez::BluetoothAgentManagerClient>(
            new bluez::FakeBluetoothAgentManagerClient));
    dbus_setter->SetBluetoothDeviceClient(
        std::unique_ptr<bluez::BluetoothDeviceClient>(
            new bluez::FakeBluetoothDeviceClient));
    dbus_setter->SetBluetoothGattServiceClient(
        std::unique_ptr<bluez::BluetoothGattServiceClient>(
            new bluez::FakeBluetoothGattServiceClient));
    dbus_setter->SetBluetoothInputClient(
        std::unique_ptr<bluez::BluetoothInputClient>(
            new bluez::FakeBluetoothInputClient));
    dbus_setter->SetBluetoothProfileManagerClient(
        std::unique_ptr<bluez::BluetoothProfileManagerClient>(
            new bluez::FakeBluetoothProfileManagerClient));

    BluetoothSocketThread::Get();

    // Grab a pointer to the adapter.
    {
      base::RunLoop run_loop;
      device::BluetoothAdapterFactory::GetAdapter(base::BindOnce(
          &BluetoothSocketBlueZTest::AdapterCallback, base::Unretained(this),
          run_loop.QuitWhenIdleClosure()));
      run_loop.Run();
    }

    ASSERT_TRUE(adapter_.get() != nullptr);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());

    // Turn on the adapter.
    adapter_->SetPowered(true, base::DoNothing(), base::DoNothing());
    ASSERT_TRUE(adapter_->IsPowered());
  }

  void TearDown() override {
    adapter_ = nullptr;
    BluetoothSocketThread::CleanupForTesting();
    bluez::BluezDBusManager::Shutdown();
  }

  void AdapterCallback(base::OnceClosure continuation,
                       scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
    std::move(continuation).Run();
  }

  void SuccessCallback(base::OnceClosure continuation) {
    ++success_callback_count_;
    std::move(continuation).Run();
  }

  void ErrorCallback(base::OnceClosure continuation,
                     const std::string& message) {
    ++error_callback_count_;
    last_message_ = message;
    std::move(continuation).Run();
  }

  void ConnectToServiceSuccessCallback(base::OnceClosure continuation,
                                       scoped_refptr<BluetoothSocket> socket) {
    ++success_callback_count_;
    last_socket_ = socket;
    std::move(continuation).Run();
  }

  void SendSuccessCallback(base::OnceClosure continuation, int bytes_sent) {
    ++success_callback_count_;
    last_bytes_sent_ = bytes_sent;
    std::move(continuation).Run();
  }

  void ReceiveSuccessCallback(base::OnceClosure continuation,
                              int bytes_received,
                              scoped_refptr<net::IOBuffer> io_buffer) {
    ++success_callback_count_;
    last_bytes_received_ = bytes_received;
    last_io_buffer_ = io_buffer;
    std::move(continuation).Run();
  }

  void ReceiveErrorCallback(base::OnceClosure continuation,
                            BluetoothSocket::ErrorReason reason,
                            const std::string& error_message) {
    ++error_callback_count_;
    last_reason_ = reason;
    last_message_ = error_message;
    std::move(continuation).Run();
  }

  void CreateServiceSuccessCallback(base::OnceClosure continuation,
                                    scoped_refptr<BluetoothSocket> socket) {
    ++success_callback_count_;
    last_socket_ = socket;
    std::move(continuation).Run();
  }

  void AcceptSuccessCallback(base::OnceClosure continuation,
                             const BluetoothDevice* device,
                             scoped_refptr<BluetoothSocket> socket) {
    ++success_callback_count_;
    last_device_ = device;
    last_socket_ = socket;
    std::move(continuation).Run();
  }

  void ImmediateSuccessCallback() { ++success_callback_count_; }

 protected:
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<BluetoothAdapter> adapter_;

  unsigned int success_callback_count_;
  unsigned int error_callback_count_;

  std::string last_message_;
  scoped_refptr<BluetoothSocket> last_socket_;
  int last_bytes_sent_;
  int last_bytes_received_;
  scoped_refptr<net::IOBuffer> last_io_buffer_;
  BluetoothSocket::ErrorReason last_reason_;
  const BluetoothDevice* last_device_;
};

TEST_F(BluetoothSocketBlueZTest, Connect) {
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);

  {
    base::RunLoop run_loop;
    device->ConnectToService(
        BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
        base::Bind(&BluetoothSocketBlueZTest::ConnectToServiceSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Send data to the socket, expect all of the data to be sent.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("test");

  {
    base::RunLoop run_loop;
    socket->Send(
        write_buffer.get(), write_buffer->size(),
        base::Bind(&BluetoothSocketBlueZTest::SendSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_EQ(last_bytes_sent_, write_buffer->size());

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Receive data from the socket, and fetch the buffer from the callback; since
  // the fake is an echo server, we expect to receive what we wrote.
  {
    base::RunLoop run_loop;
    socket->Receive(
        4096,
        base::Bind(&BluetoothSocketBlueZTest::ReceiveSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ReceiveErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_EQ(4, last_bytes_received_);
  EXPECT_TRUE(last_io_buffer_.get() != nullptr);

  // Take ownership of the received buffer.
  scoped_refptr<net::IOBuffer> read_buffer = last_io_buffer_;
  last_io_buffer_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  std::string data = std::string(read_buffer->data(), last_bytes_received_);
  EXPECT_EQ("test", data);

  read_buffer = nullptr;

  // Receive data again; the socket will have been closed, this should cause a
  // disconnected error to be returned via the error callback.
  {
    base::RunLoop run_loop;
    socket->Receive(
        4096,
        base::Bind(&BluetoothSocketBlueZTest::ReceiveSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ReceiveErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);
  EXPECT_EQ(BluetoothSocket::kDisconnected, last_reason_);
  EXPECT_EQ(net::ErrorToString(net::OK), last_message_);

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Send data again; since the socket is closed we should get a system error
  // equivalent to the connection reset error.
  write_buffer = base::MakeRefCounted<net::StringIOBuffer>("second test");

  {
    base::RunLoop run_loop;
    socket->Send(
        write_buffer.get(), write_buffer->size(),
        base::Bind(&BluetoothSocketBlueZTest::SendSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);
  EXPECT_EQ(net::ErrorToString(net::ERR_CONNECTION_RESET), last_message_);

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Close our end of the socket.
  {
    base::RunLoop run_loop;
    socket->Disconnect(base::Bind(&BluetoothSocketBlueZTest::SuccessCallback,
                                  base::Unretained(this),
                                  run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketBlueZTest, Listen) {
  {
    base::RunLoop run_loop;
    adapter_->CreateRfcommService(
        BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
        BluetoothAdapter::ServiceOptions(),
        base::Bind(&BluetoothSocketBlueZTest::CreateServiceSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> server_socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Simulate an incoming connection by just calling the ConnectProfile method
  // of the underlying fake device client (from the BlueZ point of view,
  // outgoing and incoming look the same).
  //
  // This is done before the Accept() call to simulate a pending call at the
  // point that Accept() is called.
  bluez::FakeBluetoothDeviceClient* fake_bluetooth_device_client =
      static_cast<bluez::FakeBluetoothDeviceClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient());
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  {
    base::RunLoop run_loop;
    fake_bluetooth_device_client->ConnectProfile(
        static_cast<BluetoothDeviceBlueZ*>(device)->object_path(),
        bluez::FakeBluetoothProfileManagerClient::kRfcommUuid,
        base::DoNothing(), base::DoNothing());
    run_loop.RunUntilIdle();
  }
  {
    base::RunLoop run_loop;
    server_socket->Accept(
        base::Bind(&BluetoothSocketBlueZTest::AcceptSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the client socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> client_socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Close our end of the client socket.
  {
    base::RunLoop run_loop;
    client_socket->Disconnect(
        base::Bind(&BluetoothSocketBlueZTest::SuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  client_socket = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Run a second connection test, this time calling Accept() before the
  // incoming connection comes in.
  {
    base::RunLoop run_loop1;
    // |run_loop2| is expected to be quit in ConnectProfile() through the quit
    // closures saved in the Accept() call.
    base::RunLoop run_loop2;

    server_socket->Accept(
        base::Bind(&BluetoothSocketBlueZTest::AcceptSuccessCallback,
                   base::Unretained(this), run_loop2.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop2.QuitWhenIdleClosure()));
    run_loop1.RunUntilIdle();

    fake_bluetooth_device_client->ConnectProfile(
        static_cast<BluetoothDeviceBlueZ*>(device)->object_path(),
        bluez::FakeBluetoothProfileManagerClient::kRfcommUuid,
        base::DoNothing(), base::DoNothing());
    run_loop2.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the client socket for the remainder of the test.
  client_socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Close our end of the client socket.
  {
    base::RunLoop run_loop;
    client_socket->Disconnect(
        base::Bind(&BluetoothSocketBlueZTest::SuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  client_socket = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Now close the server socket.
  {
    base::RunLoop run_loop;
    server_socket->Disconnect(
        base::Bind(&BluetoothSocketBlueZTest::ImmediateSuccessCallback,
                   base::Unretained(this)));
    run_loop.RunUntilIdle();
  }

  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketBlueZTest, ListenBeforeAdapterStart) {
  // Start off with an invisible adapter, register the profile, then make
  // the adapter visible.
  bluez::FakeBluetoothAdapterClient* fake_bluetooth_adapter_client =
      static_cast<bluez::FakeBluetoothAdapterClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient());
  fake_bluetooth_adapter_client->SetVisible(false);

  {
    base::RunLoop run_loop;
    adapter_->CreateRfcommService(
        BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
        BluetoothAdapter::ServiceOptions(),
        base::Bind(&BluetoothSocketBlueZTest::CreateServiceSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // But there shouldn't be a profile registered yet.
  bluez::FakeBluetoothProfileManagerClient*
      fake_bluetooth_profile_manager_client =
          static_cast<bluez::FakeBluetoothProfileManagerClient*>(
              bluez::BluezDBusManager::Get()
                  ->GetBluetoothProfileManagerClient());
  bluez::FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider == nullptr);

  // Make the adapter visible. This should register a profile.
  {
    base::RunLoop run_loop;
    fake_bluetooth_adapter_client->SetVisible(true);
    run_loop.RunUntilIdle();
  }

  profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider != nullptr);

  // Cleanup the socket.
  {
    base::RunLoop run_loop;
    socket->Disconnect(
        base::Bind(&BluetoothSocketBlueZTest::ImmediateSuccessCallback,
                   base::Unretained(this)));
    run_loop.RunUntilIdle();
  }

  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketBlueZTest, ListenAcrossAdapterRestart) {
  // The fake adapter starts off visible by default.
  bluez::FakeBluetoothAdapterClient* fake_bluetooth_adapter_client =
      static_cast<bluez::FakeBluetoothAdapterClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient());

  {
    base::RunLoop run_loop;
    adapter_->CreateRfcommService(
        BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
        BluetoothAdapter::ServiceOptions(),
        base::Bind(&BluetoothSocketBlueZTest::CreateServiceSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Make sure the profile was registered with the daemon.
  bluez::FakeBluetoothProfileManagerClient*
      fake_bluetooth_profile_manager_client =
          static_cast<bluez::FakeBluetoothProfileManagerClient*>(
              bluez::BluezDBusManager::Get()
                  ->GetBluetoothProfileManagerClient());
  bluez::FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider != nullptr);

  // Make the adapter invisible, and fiddle with the profile fake to unregister
  // the profile since this doesn't happen automatically.
  {
    base::RunLoop run_loop;
    fake_bluetooth_adapter_client->SetVisible(false);
    run_loop.RunUntilIdle();
  }

  // Then make the adapter visible again. This should re-register the profile.
  {
    base::RunLoop run_loop;
    fake_bluetooth_adapter_client->SetVisible(true);
    run_loop.RunUntilIdle();
  }

  profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider != nullptr);

  // Cleanup the socket.
  {
    base::RunLoop run_loop;
    socket->Disconnect(
        base::Bind(&BluetoothSocketBlueZTest::ImmediateSuccessCallback,
                   base::Unretained(this)));
    run_loop.RunUntilIdle();
  }

  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketBlueZTest, PairedConnectFails) {
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress);
  ASSERT_TRUE(device != nullptr);

  {
    base::RunLoop run_loop;
    device->ConnectToService(
        BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
        base::Bind(&BluetoothSocketBlueZTest::ConnectToServiceSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() == nullptr);

  {
    base::RunLoop run_loop;
    device->ConnectToService(
        BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
        base::Bind(&BluetoothSocketBlueZTest::ConnectToServiceSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(2U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() == nullptr);
}

TEST_F(BluetoothSocketBlueZTest, SocketListenTwice) {
  {
    base::RunLoop run_loop;
    adapter_->CreateRfcommService(
        BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
        BluetoothAdapter::ServiceOptions(),
        base::Bind(&BluetoothSocketBlueZTest::CreateServiceSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take control of this socket.
  scoped_refptr<BluetoothSocket> server_socket;
  server_socket.swap(last_socket_);

  {
    base::RunLoop run_loop;
    server_socket->Accept(
        base::Bind(&BluetoothSocketBlueZTest::AcceptSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));

    server_socket->Close();

    server_socket = nullptr;
    run_loop.RunUntilIdle();
  }

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);

  {
    base::RunLoop run_loop;
    adapter_->CreateRfcommService(
        BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
        BluetoothAdapter::ServiceOptions(),
        base::Bind(&BluetoothSocketBlueZTest::CreateServiceSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(2U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take control of this socket.
  server_socket.swap(last_socket_);

  {
    base::RunLoop run_loop;
    server_socket->Accept(
        base::Bind(&BluetoothSocketBlueZTest::AcceptSuccessCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));

    server_socket->Close();

    server_socket = nullptr;
    run_loop.RunUntilIdle();
  }

  EXPECT_EQ(2U, success_callback_count_);
  EXPECT_EQ(2U, error_callback_count_);
}

}  // namespace bluez
