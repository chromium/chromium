// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_socket_manager.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

// Matches a dbus::MethodCall based on the method name (member).
MATCHER_P(HasMemberOf, member, "") {
  return arg->GetMember() == member;
}
}  // namespace

namespace floss {

using Security = FlossSocketManager::Security;
using BtifStatus = FlossDBusClient::BtifStatus;

class FlossSocketManagerTest : public testing::Test {
 public:
  FlossSocketManagerTest() = default;

  void SetUpMocks() {
    adapter_path_ = FlossManagerClient::GenerateAdapterPath(adapter_index_);
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
    // Clean up the socket manager first to get rid of all references to various
    // buses, object proxies, etc.
    sockmgr_.reset();
  }

  void Init() {
    sockmgr_->Init(bus_.get(), kSocketManagerInterface, adapter_path_.value());
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

  void SendOutgoingConnectionResult(
      FlossSocketManager::SocketId id,
      BtifStatus status,
      absl::optional<FlossSocketManager::FlossSocket> socket,
      dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(socket_manager::kCallbackInterface,
                                 socket_manager::kOnOutgoingConnectionResult);
    method_call.SetSerial(serial_++);
    dbus::MessageWriter writer(&method_call);
    FlossDBusClient::WriteAllDBusParams(&writer, id, status, socket);

    sockmgr_->OnOutgoingConnectionResult(&method_call, std::move(response));
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
    // No-op
  }

  void SockConnectionAccepted(FlossSocketManager::FlossSocket&& socket) {
    // No -op
  }

  void ExpectNormalResponse(std::unique_ptr<dbus::Response> response) {
    EXPECT_NE(response->GetMessageType(),
              dbus::Message::MessageType::MESSAGE_ERROR);
  }

  int adapter_index_ = 2;
  int serial_ = 1;
  dbus::ObjectPath adapter_path_;
  BtifStatus last_status_;

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
               absl::optional<FlossSocketManager::FlossSocket>&& socket) {
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

    absl::optional<FlossSocketManager::FlossSocket> sock =
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
               absl::optional<FlossSocketManager::FlossSocket>&& socket) {
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

    absl::optional<FlossSocketManager::FlossSocket> sock =
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

}  // namespace floss
