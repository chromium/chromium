// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_gatt_manager_client.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "device/bluetooth/floss/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {

using testing::_;

constexpr char kTestDeviceName[] = "FlossDevice";
constexpr char kTestUuidStr[] = "00010203-0405-0607-0809-0a0b0c0d0e0f";

class FlossGattClientTest : public testing::Test,
                            public FlossGattClientObserver {
 public:
  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(std::move(options));

    gatt_manager_client_ = FlossGattManagerClient::Create();
    gatt_manager_client_->AddObserver(this);

    gatt_path_ = FlossDBusClient::GenerateGattPath(adapter_index_);
    object_proxy_ = base::MakeRefCounted<::dbus::MockObjectProxy>(
        bus_.get(), kGattInterface, gatt_path_);
    EXPECT_CALL(*bus_.get(), GetObjectProxy(kGattInterface, gatt_path_))
        .WillRepeatedly(::testing::Return(object_proxy_.get()));
  }

  void TearDown() override { gatt_manager_client_.reset(); }

  void Init() {
    gatt_manager_client_->Init(bus_.get(), kGattInterface, adapter_index_,
                               floss::version::GetMaximalSupportedVersion(),
                               base::DoNothing());
  }

  void FakeGattResponseCallback(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback cb) {
    auto response = ::dbus::Response::CreateEmpty();
    std::move(cb).Run(response.get(), /*err=*/nullptr);
  }

  void SuccessCallback(DBusResult<Void> ret) {
    // Check that there is no error
    EXPECT_TRUE(ret.has_value());
    success_callback_count_++;
  }

  void FakeGattWriteResponseCallback(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback cb) {
    // Create a fake response with GattWriteRequestStatus return value.
    auto response = ::dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    writer.AppendUint32(
        static_cast<uint32_t>(GattWriteRequestStatus::kSuccess));
    std::move(cb).Run(response.get(), /*err=*/nullptr);
  }

  void SuccessWriteCallback(DBusResult<GattWriteRequestStatus> ret) {
    // Check that there is no error and return is parsed correctly
    ASSERT_TRUE(ret.has_value());
    EXPECT_EQ(ret.value(), GattWriteRequestStatus::kSuccess);
    success_callback_count_++;
  }

  void FakeGattStatusResponseCallback(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback cb) {
    // Create a fake response with GattStatus return value.
    auto response = ::dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    writer.AppendUint32(static_cast<uint32_t>(GattStatus::kSuccess));
    std::move(cb).Run(response.get(), /*err=*/nullptr);
  }

  void SuccessGattCallback(DBusResult<GattStatus> ret) {
    // Check that there is no error
    ASSERT_TRUE(ret.has_value());
    EXPECT_EQ(ret.value(), GattStatus::kSuccess);
    success_callback_count_++;
  }

  // FlossGattClientObserver overrides
  void GattClientConnectionState(GattStatus status,
                                 int32_t client_id,
                                 bool connected,
                                 std::string address) override {
    last_connection_state_address_ = address;
  }

  void TestRegisterClient() {
    gatt_manager_client_->GattClientRegistered(GattStatus::kError, 10);
    EXPECT_EQ(gatt_manager_client_->client_id_, 0);

    gatt_manager_client_->GattClientRegistered(GattStatus::kSuccess, 10);
    EXPECT_EQ(gatt_manager_client_->client_id_, 10);

    gatt_manager_client_->GattClientRegistered(GattStatus::kSuccess, 20);
    EXPECT_EQ(gatt_manager_client_->client_id_, 10);
  }

  void TestConnectionState() {
    GattStatus success = GattStatus::kSuccess;
    last_connection_state_address_ = "";

    gatt_manager_client_->GattClientConnectionState(success, 10, false,
                                                    "12345");
    EXPECT_EQ(last_connection_state_address_, "12345");

    gatt_manager_client_->GattClientConnectionState(success, 20, false,
                                                    "23456");
    EXPECT_EQ(last_connection_state_address_, "12345");
  }

  int adapter_index_ = 0;
  dbus::ObjectPath gatt_path_;
  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockObjectProxy> object_proxy_;
  std::unique_ptr<FlossGattManagerClient> gatt_manager_client_;
  std::string last_connection_state_address_ = "";

  int success_callback_count_ = 0;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossGattClientTest> weak_ptr_factory_{this};
};

TEST_F(FlossGattClientTest, UnexpectedClientHandling) {
  TestRegisterClient();
  TestConnectionState();
}

TEST_F(FlossGattClientTest, ConnectDiscoveryDisconnect) {
  Init();

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kClientConnect), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->Connect(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, FlossDBusClient::BluetoothTransport::kAuto,
      /*is_direct=*/true);

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kReadRemoteRssi), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ReadRemoteRssi(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName);

  EXPECT_CALL(*object_proxy_.get(), CallMethodWithErrorResponse(
                                        HasMemberOf(gatt::kConfigureMtu), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ConfigureMTU(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*mtu=*/0);

  EXPECT_CALL(*object_proxy_.get(),
              CallMethodWithErrorResponse(
                  HasMemberOf(gatt::kConnectionParameterUpdate), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->UpdateConnectionParameters(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*min_interval=*/0, /*max_interval=*/0, /*latency=*/0,
      /*timeout=*/0, /*min_ce_len=*/0, /*max_ce_len=*/0);

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kDiscoverServices), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->DiscoverAllServices(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName);

  EXPECT_CALL(*object_proxy_.get(),
              CallMethodWithErrorResponse(
                  HasMemberOf(gatt::kDiscoverServiceByUuid), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->DiscoverServiceByUuid(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, device::BluetoothUUID(kTestUuidStr));

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kRefreshDevice), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->Refresh(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName);

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kClientDisconnect), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->Disconnect(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName);

  EXPECT_EQ(success_callback_count_, 8);
}

TEST_F(FlossGattClientTest, ReliableWrite) {
  Init();

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kBeginReliableWrite), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->BeginReliableWrite(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName);

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kEndReliableWrite), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->EndReliableWrite(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*execute=*/true);

  EXPECT_EQ(success_callback_count_, 2);
}

TEST_F(FlossGattClientTest, ReadWriteCharacteristic) {
  Init();

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kReadCharacteristic), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ReadCharacteristic(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*handle=*/0, AuthRequired::kNoAuth);

  EXPECT_CALL(*object_proxy_.get(),
              CallMethodWithErrorResponse(
                  HasMemberOf(gatt::kReadUsingCharacteristicUuid), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ReadUsingCharacteristicUuid(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, device::BluetoothUUID(kTestUuidStr), /*start_handle=*/0,
      /*end_handle=*/0, AuthRequired::kNoAuth);

  const std::vector<uint8_t> write_value = {0x01};
  EXPECT_CALL(*object_proxy_.get(),
              CallMethodWithErrorResponse(
                  HasMemberOf(gatt::kWriteCharacteristic), _, _))
      .WillOnce(
          Invoke(this, &FlossGattClientTest::FakeGattWriteResponseCallback));
  gatt_manager_client_->WriteCharacteristic(
      base::BindOnce(&FlossGattClientTest::SuccessWriteCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*handle=*/0, WriteType::kWrite, AuthRequired::kNoAuth,
      write_value);

  EXPECT_EQ(success_callback_count_, 3);
}

TEST_F(FlossGattClientTest, ReadWriteDescriptor) {
  Init();

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kReadDescriptor), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ReadDescriptor(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*handle=*/0, AuthRequired::kNoAuth);

  const std::vector<uint8_t> write_value = {0x01};
  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kWriteDescriptor), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->WriteDescriptor(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*handle=*/0, AuthRequired::kNoAuth, write_value);

  EXPECT_EQ(success_callback_count_, 2);
}

TEST_F(FlossGattClientTest, RegisterUnregisterNotification) {
  Init();

  EXPECT_CALL(*object_proxy_.get(),
              CallMethodWithErrorResponse(
                  HasMemberOf(gatt::kRegisterForNotification), _, _))
      .WillOnce(
          Invoke(this, &FlossGattClientTest::FakeGattStatusResponseCallback));
  gatt_manager_client_->RegisterForNotification(
      base::BindOnce(&FlossGattClientTest::SuccessGattCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*handle=*/0);

  EXPECT_CALL(*object_proxy_.get(),
              CallMethodWithErrorResponse(
                  HasMemberOf(gatt::kRegisterForNotification), _, _))
      .WillOnce(
          Invoke(this, &FlossGattClientTest::FakeGattStatusResponseCallback));
  gatt_manager_client_->UnregisterNotification(
      base::BindOnce(&FlossGattClientTest::SuccessGattCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*handle=*/0);

  EXPECT_EQ(success_callback_count_, 2);
}

TEST_F(FlossGattClientTest, ServerConnectReadSetPhyDisconnect) {
  Init();

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kServerConnect), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ServerConnect(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, FlossDBusClient::BluetoothTransport::kAuto);

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kServerReadPhy), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ServerReadPhy(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName);

  EXPECT_CALL(*object_proxy_.get(),
              CallMethodWithErrorResponse(
                  HasMemberOf(gatt::kServerSetPreferredPhy), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ServerSetPreferredPhy(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*tx_phy=*/LePhy::kPhy1m, /*rx_phy=*/LePhy::kPhy1m,
      /*phy_options=*/0);

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kServerDisconnect), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ServerDisconnect(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName);

  EXPECT_EQ(success_callback_count_, 4);
}

TEST_F(FlossGattClientTest, ServerAddRemoveClearService) {
  Init();

  GattService service;
  service.uuid = device::BluetoothUUID(kTestUuidStr);
  service.instance_id = 1;
  service.service_type = 0;
  EXPECT_CALL(*object_proxy_.get(),
              CallMethodWithErrorResponse(HasMemberOf(gatt::kAddService), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->AddService(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      service);

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kRemoveService), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->RemoveService(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      /*handle=*/0);

  EXPECT_CALL(
      *object_proxy_.get(),
      CallMethodWithErrorResponse(HasMemberOf(gatt::kClearServices), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ClearServices(base::BindOnce(
      &FlossGattClientTest::SuccessCallback, base::Unretained(this)));

  EXPECT_EQ(success_callback_count_, 3);
}

TEST_F(FlossGattClientTest, ServerSendResponseNotification) {
  Init();

  const std::vector<uint8_t> resp_value = {0x01};
  EXPECT_CALL(*object_proxy_.get(), CallMethodWithErrorResponse(
                                        HasMemberOf(gatt::kSendResponse), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->SendResponse(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*request_id=*/0, GattStatus::kSuccess, /*offset=*/0,
      resp_value);

  EXPECT_CALL(*object_proxy_.get(),
              CallMethodWithErrorResponse(
                  HasMemberOf(gatt::kServerSendNotification), _, _))
      .WillOnce(Invoke(this, &FlossGattClientTest::FakeGattResponseCallback));
  gatt_manager_client_->ServerSendNotification(
      base::BindOnce(&FlossGattClientTest::SuccessCallback,
                     base::Unretained(this)),
      kTestDeviceName, /*handle=*/0, /*confirm=*/false, resp_value);

  EXPECT_EQ(success_callback_count_, 2);
}

}  // namespace floss
