// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_battery_manager_client.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "device/bluetooth/floss/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {

class FlossBatteryManagerClientTest : public testing::Test,
                                      public FlossBatteryManagerClientObserver {
 public:
  void SetUpMocks() {
    battery_manager_path_ =
        FlossDBusClient::GenerateBatteryManagerPath(adapter_index_);
    battery_manager_object_proxy_ =
        base::MakeRefCounted<::dbus::MockObjectProxy>(
            bus_.get(), kBatteryManagerInterface, battery_manager_path_);
    exported_callbacks_ = base::MakeRefCounted<::dbus::MockExportedObject>(
        bus_.get(),
        ::dbus::ObjectPath(FlossBatteryManagerClient::kExportedCallbacksPath));

    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(kBatteryManagerInterface, battery_manager_path_))
        .WillRepeatedly(::testing::Return(battery_manager_object_proxy_.get()));
    EXPECT_CALL(*bus_.get(), GetExportedObject)
        .WillRepeatedly(::testing::Return(exported_callbacks_.get()));

    // Handle method calls on the object proxy
    ON_CALL(*battery_manager_object_proxy_.get(),
            DoCallMethodWithErrorResponse(
                HasMemberOf(battery_manager::kRegisterBatteryCallback),
                testing::_, testing::_))
        .WillByDefault(Invoke(
            this, &FlossBatteryManagerClientTest::HandleRegisterCallback));
    ON_CALL(*battery_manager_object_proxy_.get(),
            DoCallMethodWithErrorResponse(
                HasMemberOf(battery_manager::kGetBatteryInformation),
                testing::_, testing::_))
        .WillByDefault(Invoke(
            this, &FlossBatteryManagerClientTest::HandleGetBatteryInformation));
  }

  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(options);
    callback_path_ =
        ::dbus::ObjectPath(FlossBatteryManagerClient::kExportedCallbacksPath);
    client_ = FlossBatteryManagerClient::Create();
    client_->AddObserver(this);
    SetUpMocks();
  }

  void TearDown() override { client_.reset(); }

  void HandleRegisterCallback(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter msg(response.get());
    msg.AppendUint32(42);

    std::move(*cb).Run(response.get(), nullptr);
  }

  void HandleGetBatteryInformation(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter msg(response.get());

    std::move(*cb).Run(response.get(), nullptr);
  }

  void HandleGetBatteryInfo(DBusResult<absl::optional<BatterySet>> result) {
    callback_count_++;
  }

  void BatteryInfoUpdated(std::string remote_address,
                          BatterySet battery_set) override {
    callback_count_++;
  }

  // Test defined here to access private members.
  void TestInitializesCorrectly() {
    EXPECT_CALL(*battery_manager_object_proxy_.get(),
                DoCallMethodWithErrorResponse)
        .Times(testing::AnyNumber());
    // Expected specific method calls.
    EXPECT_CALL(*battery_manager_object_proxy_.get(),
                DoCallMethodWithErrorResponse(
                    HasMemberOf(battery_manager::kRegisterBatteryCallback),
                    testing::_, testing::_))
        .Times(1);
    client_->Init(bus_.get(), kBatteryManagerInterface, adapter_index_);
    EXPECT_EQ(client_->battery_manager_callback_id_, static_cast<uint32_t>(42));
  }

  void TestForwardsCallbacks() {
    EXPECT_EQ(callback_count_, 0);
    client_->Init(bus_.get(), kBatteryManagerInterface, adapter_index_);
    client_->BatteryInfoUpdated("11:11:11:11:11:11", BatterySet());
    EXPECT_EQ(callback_count_, 1);
  }

  void TestExportsDBusCorrectly() {
    // Get a reference to the callback handler
    dbus::ExportedObject::MethodCallCallback
        method_handler_on_battery_info_updated;
    EXPECT_CALL(*exported_callbacks_.get(),
                ExportMethod(battery_manager::kCallbackInterface,
                             battery_manager::kOnBatteryInfoUpdated, testing::_,
                             testing::_))
        .WillOnce(testing::SaveArg<2>(&method_handler_on_battery_info_updated));
    client_->Init(bus_.get(), kBatteryManagerInterface, adapter_index_);
    ASSERT_TRUE(!!method_handler_on_battery_info_updated);

    // Set up DBus message
    dbus::MethodCall method_call("an.interface",
                                 battery_manager::kOnBatteryInfoUpdated);
    method_call.SetPath(callback_path_);
    method_call.SetSender(":0.1");
    method_call.SetSerial(1);
    dbus::MessageWriter writer(&method_call);
    BatterySet test_battery_set{};
    std::string test_battery_address = "11:11:11:11:11:11";
    FlossDBusClient::WriteAllDBusParams(&writer, test_battery_address,
                                        test_battery_set);

    // Invoke the callback and confirm that the DBus message is correctly parsed
    // and triggers callbacks.
    EXPECT_EQ(callback_count_, 0);
    method_handler_on_battery_info_updated.Run(
        &method_call,
        base::BindOnce([](std::unique_ptr<dbus::Response> response) {
          ASSERT_TRUE(!!response);
          EXPECT_NE(response->GetMessageType(),
                    dbus::Message::MessageType::MESSAGE_ERROR);
        }));
    EXPECT_EQ(callback_count_, 1);
  }

  void TestGetBatteryInfo() {
    EXPECT_CALL(*battery_manager_object_proxy_.get(),
                DoCallMethodWithErrorResponse)
        .Times(testing::AnyNumber());
    EXPECT_CALL(*battery_manager_object_proxy_.get(),
                DoCallMethodWithErrorResponse(
                    HasMemberOf(battery_manager::kGetBatteryInformation),
                    testing::_, testing::_))
        .Times(1);
    EXPECT_EQ(callback_count_, 0);
    client_->Init(bus_.get(), kBatteryManagerInterface, adapter_index_);
    FlossDeviceId test_device{};
    test_device.address = "11:11:11:11:11:11";
    client_->GetBatteryInformation(
        base::BindOnce(&FlossBatteryManagerClientTest::HandleGetBatteryInfo,
                       weak_ptr_factory_.GetWeakPtr()),
        test_device);
    EXPECT_EQ(callback_count_, 1);
  }

  int callback_count_ = 0;
  int adapter_index_ = 5;
  dbus::ObjectPath callback_path_;
  dbus::ObjectPath battery_manager_path_;
  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockExportedObject> exported_callbacks_;
  scoped_refptr<::dbus::MockObjectProxy> battery_manager_object_proxy_;
  std::unique_ptr<FlossBatteryManagerClient> client_;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossBatteryManagerClientTest> weak_ptr_factory_{this};
};

TEST_F(FlossBatteryManagerClientTest, InitializesCorrectly) {
  TestInitializesCorrectly();
}

TEST_F(FlossBatteryManagerClientTest, ForwardsCallbacks) {
  TestForwardsCallbacks();
}

TEST_F(FlossBatteryManagerClientTest, ExportsDBusCorrectly) {
  TestExportsDBusCorrectly();
}

TEST_F(FlossBatteryManagerClientTest, GetBatteryInfo) {
  TestGetBatteryInfo();
}

}  // namespace floss
