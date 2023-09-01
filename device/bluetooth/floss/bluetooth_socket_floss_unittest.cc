// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_socket_floss.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/fake_floss_manager_client.h"
#include "device/bluetooth/floss/fake_floss_socket_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::device::BluetoothAdapter;

}  // namespace

namespace floss {

class BluetoothSocketFlossTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<floss::FlossDBusManagerSetter> dbus_setter =
        floss::FlossDBusManager::GetSetterForTesting();

    auto fake_floss_manager_client = std::make_unique<FakeFlossManagerClient>();
    fake_floss_manager_client_ = fake_floss_manager_client.get();
    dbus_setter->SetFlossManagerClient(std::move(fake_floss_manager_client));

    InitializeAndEnableAdapter();
  }

  void TearDown() override {
    adapter_ = nullptr;
    device::BluetoothSocketThread::CleanupForTesting();
  }

  void InitializeAndEnableAdapter() {
    adapter_ = BluetoothAdapterFloss::CreateAdapter();

    fake_floss_manager_client_->SetDefaultEnabled(true);

    base::RunLoop run_loop;
    adapter_->Initialize(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(adapter_);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPowered());

    ASSERT_TRUE(adapter_.get() != nullptr);

    fake_floss_manager_client_->NotifyObservers(
        base::BindLambdaForTesting([](FlossManagerClient::Observer* observer) {
          observer->AdapterEnabledChanged(/*adapter=*/0, /*enabled=*/true);
        }));
    base::RunLoop().RunUntilIdle();

    // Get the socket thread.
    device::BluetoothSocketThread::Get();
  }

  void AcceptSuccessCallback(base::OnceClosure exitloop,
                             const device::BluetoothDevice* device,
                             scoped_refptr<device::BluetoothSocket> socket) {
    success_callback_count_++;
    last_socket_ = std::move(socket);
    std::move(exitloop).Run();
  }

  void ConnectToServiceSuccessCallback(
      base::OnceClosure exitloop,
      scoped_refptr<device::BluetoothSocket> socket) {
    success_callback_count_++;
    last_socket_ = std::move(socket);
    std::move(exitloop).Run();
  }

  void CreateServiceSuccessCallback(
      base::OnceClosure exitloop,
      scoped_refptr<device::BluetoothSocket> socket) {
    success_callback_count_++;
    last_socket_ = std::move(socket);
    std::move(exitloop).Run();
  }

  void DisconnectSuccessCallback(base::OnceClosure exitloop) {
    std::move(exitloop).Run();
  }

  void ErrorCallback(base::OnceClosure exitloop, const std::string& message) {
    LOG(ERROR) << "ErrorCallback: " << message;
    error_callback_count_++;
    last_message_ = message;
    std::move(exitloop).Run();
  }

  void ImmediateSuccessCallback() { success_callback_count_++; }

  void SendSuccessCallback(base::OnceClosure exitloop, int bytes_sent) {
    ++success_callback_count_;
    last_bytes_sent_ = bytes_sent;
    std::move(exitloop).Run();
  }

  // Clear counters for test.
  void ClearCounters() {
    last_bytes_sent_ = 0;
    last_bytes_received_ = 0;
    success_callback_count_ = 0;
    error_callback_count_ = 0;
    last_socket_ = nullptr;
  }

  void DisconnectSocket(device::BluetoothSocket* socket) {
    base::RunLoop run_loop;
    socket->Disconnect(base::BindOnce(
        &BluetoothSocketFlossTest::DisconnectSuccessCallback,
        weak_ptr_factory_.GetWeakPtr(), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  FakeFlossSocketManager* GetFakeFlossSocketManager() {
    return static_cast<FakeFlossSocketManager*>(
        FlossDBusManager::Get()->GetSocketManager());
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<BluetoothAdapter> adapter_;

  std::string last_message_;
  int last_bytes_sent_ = 0;
  int last_bytes_received_ = 0;
  int success_callback_count_ = 0;
  int error_callback_count_ = 0;
  scoped_refptr<device::BluetoothSocket> last_socket_;

  // Holds pointer to FakeFloss*Client's so that we can manipulate the fake
  // within tests.
  raw_ptr<FakeFlossManagerClient> fake_floss_manager_client_;

  base::WeakPtrFactory<BluetoothSocketFlossTest> weak_ptr_factory_{this};
};

TEST_F(BluetoothSocketFlossTest, Connect) {
  device::BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1);
  ASSERT_TRUE(device != nullptr);

  // First establish a connection.
  {
    base::RunLoop run_loop;
    device->ConnectToService(
        device::BluetoothUUID(FakeFlossSocketManager::kRfcommUuid),
        base::BindOnce(
            &BluetoothSocketFlossTest::ConnectToServiceSuccessCallback,
            weak_ptr_factory_.GetWeakPtr(), run_loop.QuitWhenIdleClosure()),
        base::BindOnce(&BluetoothSocketFlossTest::ErrorCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of socket.
  scoped_refptr<device::BluetoothSocket> socket = std::move(last_socket_);
  ClearCounters();

  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("test");
  {
    base::RunLoop run_loop;
    socket->Send(write_buffer.get(), write_buffer->size(),
                 base::BindOnce(&BluetoothSocketFlossTest::SendSuccessCallback,
                                weak_ptr_factory_.GetWeakPtr(),
                                run_loop.QuitWhenIdleClosure()),
                 base::BindOnce(&BluetoothSocketFlossTest::ErrorCallback,
                                weak_ptr_factory_.GetWeakPtr(),
                                run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(last_bytes_sent_, write_buffer->size());
  ClearCounters();

  // Clean up the socket
  DisconnectSocket(socket.get());
  socket = nullptr;
}

// TODO (crbug.com/1412530) Test is failing on ASan bots
TEST_F(BluetoothSocketFlossTest, Listen) {
  // Get socket id for next returned socket.
  FlossSocketManager::SocketId id = GetFakeFlossSocketManager()->GetNextId();

  // First create the service.
  {
    base::RunLoop run_loop;
    adapter_->CreateRfcommService(
        device::BluetoothUUID(FakeFlossSocketManager::kRfcommUuid),
        BluetoothAdapter::ServiceOptions(),
        base::BindOnce(&BluetoothSocketFlossTest::CreateServiceSuccessCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()),
        base::BindOnce(&BluetoothSocketFlossTest::ErrorCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()));
    // Mark the socket as ready. This should trigger the success callback and an
    // accept.
    GetFakeFlossSocketManager()->SendSocketReady(
        id, device::BluetoothUUID(FakeFlossSocketManager::kRfcommUuid),
        FlossDBusClient::BtifStatus::kSuccess);
    run_loop.Run();
  }

  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of server socket.
  scoped_refptr<device::BluetoothSocket> server_socket =
      std::move(last_socket_);
  ClearCounters();

  // Simulate incoming connection. This queues one up to be accepted later.
  FlossDeviceId device = {.address = FakeFlossAdapterClient::kBondedAddress1,
                          .name = "Foobar"};
  GetFakeFlossSocketManager()->SendIncomingConnection(
      id, device, device::BluetoothUUID(FakeFlossSocketManager::kRfcommUuid));

  // Accept a connection and verify there is something there.
  {
    base::RunLoop run_loop;
    server_socket->Accept(
        base::BindOnce(&BluetoothSocketFlossTest::AcceptSuccessCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()),
        base::BindOnce(&BluetoothSocketFlossTest::ErrorCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the client socket and close it.
  scoped_refptr<device::BluetoothSocket> client_socket =
      std::move(last_socket_);
  ClearCounters();

  DisconnectSocket(client_socket.get());
  client_socket = nullptr;
  ClearCounters();

  // Accept a connection when there's nothing there and then receives connection
  // failed.
  {
    base::RunLoop run_loop;
    server_socket->Accept(
        base::BindOnce(&BluetoothSocketFlossTest::AcceptSuccessCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()),
        base::BindOnce(&BluetoothSocketFlossTest::ErrorCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()));
    run_loop.RunUntilIdle();

    // No sockets found to accept.
    EXPECT_EQ(0, success_callback_count_);
    EXPECT_EQ(0, error_callback_count_);

    GetFakeFlossSocketManager()->SendSocketReady(
        id, device::BluetoothUUID(FakeFlossSocketManager::kRfcommUuid),
        FlossDBusClient::BtifStatus::kFail);

    EXPECT_EQ(1, error_callback_count_);
    EXPECT_EQ(0, success_callback_count_);
    EXPECT_TRUE(last_socket_.get() == nullptr);
    ClearCounters();
  }

  // Accept a connection when there's nothing there and then send connection.
  {
    // First runloop will push the accept callbacks into socket.
    // Second runloop will actually trigger when incoming connection occurs.
    base::RunLoop outer_loop;
    base::RunLoop inner_loop;

    server_socket->Accept(
        base::BindOnce(&BluetoothSocketFlossTest::AcceptSuccessCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       inner_loop.QuitWhenIdleClosure()),
        base::BindOnce(&BluetoothSocketFlossTest::ErrorCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       inner_loop.QuitWhenIdleClosure()));
    outer_loop.RunUntilIdle();

    // No sockets found to accept.
    EXPECT_EQ(0, success_callback_count_);
    EXPECT_EQ(0, error_callback_count_);
    EXPECT_TRUE(last_socket_.get() == nullptr);

    GetFakeFlossSocketManager()->SendIncomingConnection(
        id, device, device::BluetoothUUID(FakeFlossSocketManager::kRfcommUuid));
    inner_loop.Run();

    EXPECT_EQ(0, error_callback_count_);
    EXPECT_EQ(1, success_callback_count_);
    EXPECT_TRUE(last_socket_.get() != nullptr);

    // Disconnect last connecting socket
    client_socket = std::move(last_socket_);
    DisconnectSocket(client_socket.get());
    client_socket = nullptr;
    last_socket_ = nullptr;
  }

  // Try to accept twice and make sure the second one fails.
  ClearCounters();
  {
    base::RunLoop run_loop;
    server_socket->Accept(
        base::BindOnce(&BluetoothSocketFlossTest::AcceptSuccessCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()),
        base::BindOnce(&BluetoothSocketFlossTest::ErrorCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()));
    run_loop.RunUntilIdle();

    // No change after first one
    EXPECT_EQ(0, error_callback_count_);

    server_socket->Accept(
        base::BindOnce(&BluetoothSocketFlossTest::AcceptSuccessCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()),
        base::BindOnce(&BluetoothSocketFlossTest::ErrorCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()));
    run_loop.RunUntilIdle();

    // Second one should fail
    EXPECT_EQ(1, error_callback_count_);
    ClearCounters();
  }

  // Clean up server socket at end.
  DisconnectSocket(server_socket.get());
}

}  // namespace floss
