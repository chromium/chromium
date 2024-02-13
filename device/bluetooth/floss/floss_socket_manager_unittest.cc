// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_socket_manager.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"
#include "device/bluetooth/floss/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

}  // namespace

namespace floss {

using Security = FlossSocketManager::Security;
using BtifStatus = FlossDBusClient::BtifStatus;

class FlossSocketManagerTest : public testing::Test {
 public:
  FlossSocketManagerTest() = default;

  base::Version GetCurrVersion() {
    return floss::version::GetMaximalSupportedVersion();
  }

  void SetUpMocks() {
    adapter_path_ = FlossDBusClient::GenerateAdapterPath(adapter_index_);
    sockmgr_proxy_ = base::MakeRefCounted<::dbus::MockObjectProxy>(
        bus_.get(), kSocketManagerInterface, adapter_path_);
    exported_callbacks_ = base::MakeRefCounted<::dbus::MockExportedObject>(
        bus_.get(),
        ::dbus::ObjectPath(FlossSocketManager::kExportedCallbacksPath));

    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(kSocketManagerInterface, adapter_path_))
        .WillRepeatedly(::testing::Return(sockmgr_proxy_.get()));
    EXPECT_CALL(*bus_.get(), GetExportedObject)
        .WillRepeatedly(::testing::Return(exported_callbacks_.get()));

    // Make sure we export all callbacks. This will need to be updated once new
    // callbacks are added.
    EXPECT_CALL(*exported_callbacks_.get(), ExportMethod).Times(4);

    // Handle method calls on the object proxy.
    ON_CALL(*sockmgr_proxy_.get(),
            DoCallMethodWithErrorResponse(
                HasMemberOf(socket_manager::kRegisterCallback), _, _))
        .WillByDefault(
            Invoke(this, &FlossSocketManagerTest::HandleRegisterCallback));
  }

  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(options);
    sockmgr_ = FlossSocketManager::Create();

    SetUpMocks();
  }

  void TearDown() override {
    // Expected call to UnregisterCallback when client is destroyed
    EXPECT_CALL(*sockmgr_proxy_.get(),
                DoCallMethodWithErrorResponse(
                    HasMemberOf(socket_manager::kUnregisterCallback), _, _))
        .WillOnce([this](::dbus::MethodCall* method_call, int timeout_ms,
                         ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
          dbus::MessageReader msg(method_call);
          // D-Bus method call should have 1 parameter.
          uint32_t param1;
          ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
          EXPECT_EQ(this->callback_id_ctr_ - 1, param1);
          EXPECT_FALSE(msg.HasMoreData());
        });

    // Clean up the socket manager first to get rid of all references to various
    // buses, object proxies, etc.
    sockmgr_.reset();
  }

  void Init() {
    sockmgr_->Init(bus_.get(), kSocketManagerInterface, adapter_index_,
                   GetCurrVersion(), base::DoNothing());
  }

  void SetupListeningSocket() {
    // First listen on something. This will push the socket callbacks into a
    // map.
    EXPECT_CALL(
        *sockmgr_proxy_.get(),
        DoCallMethodWithErrorResponse(
            HasMemberOf(socket_manager::kListenUsingRfcommWithServiceRecord), _,
            _))
        .WillOnce(
            Invoke(this, &FlossSocketManagerTest::HandleReturnSocketResult));

    sockmgr_->ListenUsingRfcomm(
        "Foo", device::BluetoothUUID("F00D"), Security::kSecure,
        base::BindOnce(&FlossSocketManagerTest::SockStatusCb,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&FlossSocketManagerTest::SockConnectionStateChanged,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&FlossSocketManagerTest::SockConnectionAccepted,
                            weak_ptr_factory_.GetWeakPtr()));

    // We should call accept here but that state is tracked on the daemon side.
    // Opting not to simply because we have it mocked away...
  }

  void HandleRegisterCallback(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter msg(response.get());
    FlossDBusClient::WriteAllDBusParams(&msg, callback_id_ctr_);

    // Increment callback counter for next call.
    callback_id_ctr_++;

    std::move(*cb).Run(response.get(), nullptr);
  }

  void HandleReturnSocketResult(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter msg(response.get());
    FlossSocketManager::SocketResult result = {
        .status = BtifStatus::kSuccess,
        .id = socket_id_ctr_,
    };
    FlossDBusClient::WriteAllDBusParams(&msg, result);

    socket_id_ctr_++;

    std::move(*cb).Run(response.get(), nullptr);
  }

  void HandleReturnSuccess(::dbus::MethodCall* method_call,
                           int timeout_ms,
                           ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter msg(response.get());

    BtifStatus status = BtifStatus::kSuccess;
    FlossDBusClient::WriteAllDBusParams(&msg, status);

    std::move(*cb).Run(response.get(), nullptr);
  }

  void SendOutgoingConnectionResult(
      FlossSocketManager::SocketId id,
      BtifStatus status,
      const std::optional<FlossSocketManager::FlossSocket>& socket,
      dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(socket_manager::kCallbackInterface,
                                 socket_manager::kOnOutgoingConnectionResult);
    method_call.SetSerial(serial_++);
    dbus::MessageWriter writer(&method_call);
    FlossDBusClient::WriteAllDBusParams(&writer, id, status, socket);

    sockmgr_->OnOutgoingConnectionResult(&method_call, std::move(response));
  }

  void SendIncomingSocketReady(
      const FlossSocketManager::FlossListeningSocket& server_socket,
      BtifStatus status,
      dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(socket_manager::kCallbackInterface,
                                 socket_manager::kOnIncomingSocketReady);
    method_call.SetSerial(serial_++);
    dbus::MessageWriter writer(&method_call);
    FlossDBusClient::WriteAllDBusParams(&writer, server_socket, status);

    sockmgr_->OnIncomingSocketReady(&method_call, std::move(response));
  }

  void SendIncomingSocketClosed(FlossSocketManager::SocketId id,
                                BtifStatus status,
                                dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(socket_manager::kCallbackInterface,
                                 socket_manager::kOnIncomingSocketReady);
    method_call.SetSerial(serial_++);
    dbus::MessageWriter writer(&method_call);
    FlossDBusClient::WriteAllDBusParams(&writer, id, status);

    sockmgr_->OnIncomingSocketClosed(&method_call, std::move(response));
  }

  void SendIncomingConnection(FlossSocketManager::SocketId id,
                              const FlossSocketManager::FlossSocket& socket,
                              dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(socket_manager::kCallbackInterface,
                                 socket_manager::kOnIncomingSocketReady);
    method_call.SetSerial(serial_++);
    dbus::MessageWriter writer(&method_call);
    FlossDBusClient::WriteAllDBusParams(&writer, id, socket);

    sockmgr_->OnHandleIncomingConnection(&method_call, std::move(response));
  }

  void SockStatusCb(DBusResult<BtifStatus> result) {
    if (!result.has_value()) {
      last_status_ = BtifStatus::kFail;
    } else {
      last_status_ = *result;
    }
  }

  void SockConnectionStateChanged(
      FlossSocketManager::ServerSocketState state,
      FlossSocketManager::FlossListeningSocket socket,
      BtifStatus status) {
    last_state_ = state;
    last_server_socket_ = socket;
    last_status_ = status;
  }

  void SockConnectionAccepted(FlossSocketManager::FlossSocket&& socket) {
    last_incoming_socket_ = std::move(socket);
  }

  void ExpectNormalResponse(std::unique_ptr<dbus::Response> response) {
    EXPECT_NE(response->GetMessageType(),
              dbus::Message::MessageType::MESSAGE_ERROR);
  }

  int adapter_index_ = 2;
  int serial_ = 1;
  dbus::ObjectPath adapter_path_;

  FlossSocketManager::ServerSocketState last_state_;
  FlossSocketManager::FlossListeningSocket last_server_socket_;
  BtifStatus last_status_;
  FlossSocketManager::FlossSocket last_incoming_socket_;

  uint32_t callback_id_ctr_ = 1;
  uint64_t socket_id_ctr_ = 1;

  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockExportedObject> exported_callbacks_;
  scoped_refptr<::dbus::MockObjectProxy> sockmgr_proxy_;
  std::unique_ptr<FlossSocketManager> sockmgr_;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossSocketManagerTest> weak_ptr_factory_{this};
};

// Tests for good path

TEST_F(FlossSocketManagerTest, ListenOnSockets) {
  Init();

  std::map<std::string, Security> l2cap_apis = {
      {socket_manager::kListenUsingInsecureL2capChannel, Security::kInsecure},
      {socket_manager::kListenUsingL2capChannel, Security::kSecure},
  };
  std::map<std::string, Security> l2cap_le_apis = {
      {socket_manager::kListenUsingInsecureL2capLeChannel, Security::kInsecure},
      {socket_manager::kListenUsingL2capLeChannel, Security::kSecure},
  };
  std::map<std::string, Security> rfcomm_apis = {
      {socket_manager::kListenUsingInsecureRfcommWithServiceRecord,
       Security::kInsecure},
      {socket_manager::kListenUsingRfcommWithServiceRecord, Security::kSecure},
  };

  // Exercise all security paths.
  for (auto kv : l2cap_apis) {
    EXPECT_CALL(*sockmgr_proxy_.get(),
                DoCallMethodWithErrorResponse(HasMemberOf(kv.first), _, _))
        .WillOnce(
            Invoke(this, &FlossSocketManagerTest::HandleReturnSocketResult));

    last_status_ = BtifStatus::kNotReady;
    sockmgr_->ListenUsingL2cap(
        kv.second,
        base::BindOnce(&FlossSocketManagerTest::SockStatusCb,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&FlossSocketManagerTest::SockConnectionStateChanged,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&FlossSocketManagerTest::SockConnectionAccepted,
                            weak_ptr_factory_.GetWeakPtr()));

    EXPECT_EQ(BtifStatus::kSuccess, last_status_);
  }
  for (auto kv : l2cap_le_apis) {
    EXPECT_CALL(*sockmgr_proxy_.get(),
                DoCallMethodWithErrorResponse(HasMemberOf(kv.first), _, _))
        .WillOnce(
            Invoke(this, &FlossSocketManagerTest::HandleReturnSocketResult));

    last_status_ = BtifStatus::kNotReady;
    sockmgr_->ListenUsingL2capLe(
        kv.second,
        base::BindOnce(&FlossSocketManagerTest::SockStatusCb,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&FlossSocketManagerTest::SockConnectionStateChanged,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&FlossSocketManagerTest::SockConnectionAccepted,
                            weak_ptr_factory_.GetWeakPtr()));

    EXPECT_EQ(BtifStatus::kSuccess, last_status_);
  }

  for (auto kv : rfcomm_apis) {
    EXPECT_CALL(*sockmgr_proxy_.get(),
                DoCallMethodWithErrorResponse(HasMemberOf(kv.first), _, _))
        .WillOnce(
            Invoke(this, &FlossSocketManagerTest::HandleReturnSocketResult));

    last_status_ = BtifStatus::kNotReady;
    sockmgr_->ListenUsingRfcomm(
        "Foo", device::BluetoothUUID(), kv.second,
        base::BindOnce(&FlossSocketManagerTest::SockStatusCb,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&FlossSocketManagerTest::SockConnectionStateChanged,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&FlossSocketManagerTest::SockConnectionAccepted,
                            weak_ptr_factory_.GetWeakPtr()));

    EXPECT_EQ(BtifStatus::kSuccess, last_status_);
  }
}

TEST_F(FlossSocketManagerTest, ConnectToSockets) {
  Init();

  std::map<std::string, Security> l2cap_apis = {
      {socket_manager::kCreateInsecureL2capChannel, Security::kInsecure},
      {socket_manager::kCreateL2capChannel, Security::kSecure},
  };

  std::map<std::string, Security> l2cap_le_apis = {
      {socket_manager::kCreateInsecureL2capLeChannel, Security::kInsecure},
      {socket_manager::kCreateL2capLeChannel, Security::kSecure},
  };

  std::map<std::string, Security> rfcomm_apis = {
      {socket_manager::kCreateInsecureRfcommSocketToServiceRecord,
       Security::kInsecure},
      {socket_manager::kCreateRfcommSocketToServiceRecord, Security::kSecure},
  };

  FlossDeviceId remote_device = {
      .address = "00:11:22:33:44:55",
      .name = "Remote device",
  };

  int psm = 42;

  device::BluetoothUUID uuid("f0de");

  for (auto kv : l2cap_apis) {
    EXPECT_CALL(*sockmgr_proxy_.get(),
                DoCallMethodWithErrorResponse(HasMemberOf(kv.first), _, _))
        .WillOnce(
            Invoke(this, &FlossSocketManagerTest::HandleReturnSocketResult));

    bool callback_completed = false;
    BtifStatus callback_status = BtifStatus::kNotReady;
    int found_psm = -1;

    sockmgr_->ConnectUsingL2cap(
        remote_device, psm, kv.second,
        base::BindOnce(
            [](bool* complete, BtifStatus* cb_status, int* fpsm,
               BtifStatus status,
               std::optional<FlossSocketManager::FlossSocket>&& socket) {
              *complete = true;
              *cb_status = status;
              if (socket) {
                *fpsm = socket->port;
              }
            },
            &callback_completed, &callback_status, &found_psm));

    // Status shouldn't be updated yet since we get callback update AFTER we
    // send outgoing result.
    EXPECT_FALSE(callback_completed);
    EXPECT_EQ(BtifStatus::kNotReady, callback_status);

    std::optional<FlossSocketManager::FlossSocket> sock =
        FlossSocketManager::FlossSocket();
    sock->id = socket_id_ctr_ - 1;
    sock->port = psm;

    // Trigger the callback completion. We don't care about socket itself.
    SendOutgoingConnectionResult(
        socket_id_ctr_ - 1, BtifStatus::kSuccess, std::move(sock),
        base::BindOnce(&FlossSocketManagerTest::ExpectNormalResponse,
                       weak_ptr_factory_.GetWeakPtr()));

    EXPECT_TRUE(callback_completed);
    EXPECT_EQ(BtifStatus::kSuccess, callback_status);
    EXPECT_EQ(psm, found_psm);
  }

  for (auto kv : l2cap_le_apis) {
    EXPECT_CALL(*sockmgr_proxy_.get(),
                DoCallMethodWithErrorResponse(HasMemberOf(kv.first), _, _))
        .WillOnce(
            Invoke(this, &FlossSocketManagerTest::HandleReturnSocketResult));

    bool callback_completed = false;
    BtifStatus callback_status = BtifStatus::kNotReady;
    int found_psm = -1;

    sockmgr_->ConnectUsingL2capLe(
        remote_device, psm, kv.second,
        base::BindOnce(
            [](bool* complete, BtifStatus* cb_status, int* fpsm,
               BtifStatus status,
               std::optional<FlossSocketManager::FlossSocket>&& socket) {
              *complete = true;
              *cb_status = status;
              if (socket) {
                *fpsm = socket->port;
              }
            },
            &callback_completed, &callback_status, &found_psm));

    // Status shouldn't be updated yet since we get callback update AFTER we
    // send outgoing result.
    EXPECT_FALSE(callback_completed);
    EXPECT_EQ(BtifStatus::kNotReady, callback_status);

    std::optional<FlossSocketManager::FlossSocket> sock =
        FlossSocketManager::FlossSocket();
    sock->id = socket_id_ctr_ - 1;
    sock->port = psm;

    // Trigger the callback completion. We don't care about socket itself.
    SendOutgoingConnectionResult(
        socket_id_ctr_ - 1, BtifStatus::kSuccess, std::move(sock),
        base::BindOnce(&FlossSocketManagerTest::ExpectNormalResponse,
                       weak_ptr_factory_.GetWeakPtr()));

    EXPECT_TRUE(callback_completed);
    EXPECT_EQ(BtifStatus::kSuccess, callback_status);
    EXPECT_EQ(psm, found_psm);
  }

  for (auto kv : rfcomm_apis) {
    EXPECT_CALL(*sockmgr_proxy_.get(),
                DoCallMethodWithErrorResponse(HasMemberOf(kv.first), _, _))
        .WillOnce(
            Invoke(this, &FlossSocketManagerTest::HandleReturnSocketResult));

    bool callback_completed = false;
    BtifStatus callback_status = BtifStatus::kNotReady;
    device::BluetoothUUID found_uuid;

    sockmgr_->ConnectUsingRfcomm(
        remote_device, uuid, kv.second,
        base::BindOnce(
            [](bool* complete, BtifStatus* cb_status, device::BluetoothUUID* uu,
               BtifStatus status,
               std::optional<FlossSocketManager::FlossSocket>&& socket) {
              *complete = true;
              *cb_status = status;
              if (socket && socket->uuid) {
                *uu = *socket->uuid;
              }
            },
            &callback_completed, &callback_status, &found_uuid));

    // Status shouldn't be updated yet since we get callback update AFTER we
    // send outgoing result.
    EXPECT_FALSE(callback_completed);
    EXPECT_EQ(BtifStatus::kNotReady, callback_status);

    std::optional<FlossSocketManager::FlossSocket> sock =
        FlossSocketManager::FlossSocket();
    sock->id = socket_id_ctr_ - 1;
    sock->uuid = uuid;

    // Trigger the callback completion. We don't care about socket itself.
    SendOutgoingConnectionResult(
        socket_id_ctr_ - 1, BtifStatus::kSuccess, std::move(sock),
        base::BindOnce(&FlossSocketManagerTest::ExpectNormalResponse,
                       weak_ptr_factory_.GetWeakPtr()));

    EXPECT_TRUE(callback_completed);
    EXPECT_EQ(BtifStatus::kSuccess, callback_status);
    EXPECT_EQ(uuid, found_uuid);
  }
}

// Really basic calls to accept and close
TEST_F(FlossSocketManagerTest, AcceptAndCloseConnection) {
  Init();

  EXPECT_CALL(
      *sockmgr_proxy_.get(),
      DoCallMethodWithErrorResponse(HasMemberOf(socket_manager::kAccept), _, _))
      .WillOnce(Invoke(this, &FlossSocketManagerTest::HandleReturnSuccess));

  last_status_ = BtifStatus::kNotReady;
  sockmgr_->Accept(socket_id_ctr_, 42,
                   base::BindOnce(&FlossSocketManagerTest::SockStatusCb,
                                  weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(BtifStatus::kSuccess, last_status_);

  EXPECT_CALL(
      *sockmgr_proxy_.get(),
      DoCallMethodWithErrorResponse(HasMemberOf(socket_manager::kClose), _, _))
      .WillOnce(Invoke(this, &FlossSocketManagerTest::HandleReturnSuccess));

  last_status_ = BtifStatus::kNotReady;
  sockmgr_->Close(socket_id_ctr_,
                  base::BindOnce(&FlossSocketManagerTest::SockStatusCb,
                                 weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(BtifStatus::kSuccess, last_status_);
}

// Handle state changes from calling accept and close.
TEST_F(FlossSocketManagerTest, IncomingStateChanges) {
  Init();
  SetupListeningSocket();

  // With a bad id, callbacks will never be dispatched.
  FlossSocketManager::FlossListeningSocket bad_socket;
  bad_socket.id = 123456789;

  // Good id is the last socket ctr we used.
  FlossSocketManager::FlossListeningSocket good_socket;
  good_socket.id = socket_id_ctr_ - 1;
  good_socket.name = "Foo";
  good_socket.uuid = device::BluetoothUUID("F00D");

  // Empty out the last seen status and socket.
  last_status_ = BtifStatus::kNotReady;
  last_server_socket_ = FlossSocketManager::FlossListeningSocket();
  last_state_ = FlossSocketManager::ServerSocketState::kClosed;

  EXPECT_FALSE(last_server_socket_.is_valid());
  // Send an invalid update. Should result in no callbacks being called.
  SendIncomingSocketReady(
      bad_socket, BtifStatus::kSuccess,
      base::BindOnce(&FlossSocketManagerTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(BtifStatus::kNotReady, last_status_);
  EXPECT_FALSE(last_server_socket_.is_valid());

  // Send a successful ready to a valid socket.
  SendIncomingSocketReady(
      good_socket, BtifStatus::kSuccess,
      base::BindOnce(&FlossSocketManagerTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(BtifStatus::kSuccess, last_status_);
  EXPECT_EQ(last_state_, FlossSocketManager::ServerSocketState::kReady);
  EXPECT_TRUE(last_server_socket_.is_valid());

  // Empty out the last seen status and socket.
  last_status_ = BtifStatus::kNotReady;
  last_server_socket_ = FlossSocketManager::FlossListeningSocket();

  // Send an invalid update. Should result in no callbacks being called.
  SendIncomingSocketClosed(
      bad_socket.id, BtifStatus::kSuccess,
      base::BindOnce(&FlossSocketManagerTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(BtifStatus::kNotReady, last_status_);
  EXPECT_FALSE(last_server_socket_.is_valid());

  // Send a successful close to a valid socket.
  SendIncomingSocketClosed(
      good_socket.id, BtifStatus::kSuccess,
      base::BindOnce(&FlossSocketManagerTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(BtifStatus::kSuccess, last_status_);
  EXPECT_EQ(last_server_socket_.id, good_socket.id);
  EXPECT_EQ(last_state_, FlossSocketManager::ServerSocketState::kClosed);

  // Try sending a ready to the same socket and nothing should happen.
  last_status_ = BtifStatus::kNotReady;
  SendIncomingSocketReady(
      good_socket, BtifStatus::kSuccess,
      base::BindOnce(&FlossSocketManagerTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(BtifStatus::kNotReady, last_status_);
}

// Handle incoming socket connections.
TEST_F(FlossSocketManagerTest, IncomingConnections) {
  Init();
  SetupListeningSocket();

  // With a bad id, callbacks will never be dispatched.
  FlossSocketManager::FlossSocket bad_socket;
  bad_socket.id = 123456789;

  // Good id is the last socket ctr we used.
  FlossSocketManager::FlossSocket good_socket;
  good_socket.id = socket_id_ctr_ - 1;

  last_incoming_socket_ = FlossSocketManager::FlossSocket();
  EXPECT_FALSE(last_incoming_socket_.is_valid());

  SendIncomingConnection(
      bad_socket.id, bad_socket,
      base::BindOnce(&FlossSocketManagerTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_FALSE(last_incoming_socket_.is_valid());

  SendIncomingConnection(
      good_socket.id, good_socket,
      base::BindOnce(&FlossSocketManagerTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(last_incoming_socket_.is_valid());
  EXPECT_EQ(last_incoming_socket_.id, good_socket.id);
}

}  // namespace floss
