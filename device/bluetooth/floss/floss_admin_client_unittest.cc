// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/floss/floss_admin_client.h"

#include <map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/device_event_log/device_event_log.h"
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

namespace floss {
namespace {

using testing::_;
using testing::DoAll;

const uint32_t kTestCallbackId = 1000;
constexpr size_t kUUIDSize = 16;
const uint8_t kTestUuidInBytes[][kUUIDSize] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
};

const std::vector<device::BluetoothUUID> kTestUuidStr = {
    device::BluetoothUUID("00010203-0405-0607-0809-0a0b0c0d0e0f"),
    device::BluetoothUUID("0f0e0d0c-0b0a-0908-0706-050403020100"),
};

void FakeExportMethod(
    const std::string& interface_name,
    const std::string& method_name,
    const dbus::ExportedObject::MethodCallCallback& method_call_callback,
    dbus::ExportedObject::OnExportedCallback on_exported_callback) {
  std::move(on_exported_callback)
      .Run(interface_name, method_name, /*success=*/true);
}

}  // namespace

class FlossAdminClientTest : public testing::Test,
                             public FlossAdminClientObserver {
 public:
  FlossAdminClientTest() = default;

  base::Version GetCurrVersion() {
    return floss::version::GetMaximalSupportedVersion();
  }

  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(options);
    client_ = FlossAdminClient::Create();
    client_->AddObserver(this);

    admin_path_ = FlossDBusClient::GenerateAdminPath(adapter_index_);
    callback_path_ = dbus::ObjectPath(FlossAdminClient::kExportedCallbacksPath);

    object_proxy_ = base::MakeRefCounted<::dbus::MockObjectProxy>(
        bus_.get(), kAdapterInterface, admin_path_);

    EXPECT_CALL(*bus_.get(), GetObjectProxy(kAdapterInterface, admin_path_))
        .WillRepeatedly(::testing::Return(object_proxy_.get()));
  }

  void TearDown() override {
    // Clean up the client first so it gets rid of all its references to the
    // various buses, object proxies, etc.
    client_.reset();
  }

  // AdminClientObserver overrides
  void DevicePolicyEffectChanged(
      const FlossDeviceId& device_id,
      const std::optional<PolicyEffect>& effect) override {
    fake_device_policy_effect_info_ = {device_id, effect};
  }

  void ServiceAllowlistChanged(
      const std::vector<device::BluetoothUUID>& allowlist) override {
    fake_service_allowlist_info_ = allowlist;
  }

  bool IsClientRegistered() { return client_->IsClientRegistered(); }

  void TestInit() {
    scoped_refptr<::dbus::MockExportedObject> exported_callback =
        base::MakeRefCounted<::dbus::MockExportedObject>(bus_.get(),
                                                         callback_path_);
    // Expected exported callbacks
    dbus::ExportedObject::MethodCallCallback
        method_handler_on_device_policy_effect_changed;
    EXPECT_CALL(*exported_callback.get(),
                ExportMethod(admin::kCallbackInterface,
                             admin::kOnDevicePolicyEffectChanged, _, _))
        .WillOnce(DoAll(testing::SaveArg<2>(
                            &method_handler_on_device_policy_effect_changed),
                        &FakeExportMethod));

    dbus::ExportedObject::MethodCallCallback
        method_handler_on_service_allowlist_changed;
    EXPECT_CALL(*exported_callback.get(),
                ExportMethod(admin::kCallbackInterface,
                             admin::kOnServiceAllowlistChanged, _, _))
        .WillOnce(DoAll(
            testing::SaveArg<2>(&method_handler_on_service_allowlist_changed),
            &FakeExportMethod));

    EXPECT_CALL(*bus_.get(), GetExportedObject(callback_path_))
        .WillRepeatedly(testing::Return(exported_callback.get()));

    // Expected call to RegisterAdminCallback when client is initialized
    EXPECT_CALL(*object_proxy_.get(),
                DoCallMethodWithErrorResponse(
                    HasMemberOf(admin::kRegisterCallback), _, _))
        .WillOnce([this](::dbus::MethodCall* method_call, int timeout_ms,
                         ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
          dbus::MessageReader msg(method_call);
          // D-Bus method call should have 1 parameter.
          dbus::ObjectPath param1;
          ASSERT_TRUE(msg.PopObjectPath(&param1));
          EXPECT_EQ(param1, this->callback_path_);
          EXPECT_FALSE(msg.HasMoreData());
          // Create a fake response with uint32_t return value.
          auto response = ::dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendUint32(kTestCallbackId);
          std::move(*cb).Run(response.get(), /*err=*/nullptr);
        });
    ASSERT_FALSE(IsClientRegistered());
    client_->Init(bus_.get(), kAdapterInterface, adapter_index_,
                  GetCurrVersion(), base::DoNothing());

    // Test exported callbacks are correctly parsed
    ASSERT_TRUE(!!method_handler_on_device_policy_effect_changed);
    ASSERT_TRUE(!!method_handler_on_service_allowlist_changed);
    ASSERT_TRUE(IsClientRegistered());

    // Expected call to UnregisterAdminCallback when client is destroyed
    EXPECT_CALL(*object_proxy_.get(),
                DoCallMethodWithErrorResponse(
                    HasMemberOf(admin::kUnregisterCallback), _, _))
        .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                     ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
          dbus::MessageReader msg(method_call);
          // D-Bus method call should have 1 parameter.
          uint32_t param1;
          ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
          EXPECT_EQ(kTestCallbackId, param1);
          EXPECT_FALSE(msg.HasMoreData());
        });
  }

  void TestSetServiceAllowlist() {
    // Expected call to SetAllowedServices
    EXPECT_CALL(*object_proxy_.get(),
                DoCallMethodWithErrorResponse(
                    HasMemberOf(admin::kSetAllowedServices), _, _))
        .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                     ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
          dbus::MessageReader reader(method_call);
          dbus::MessageReader array_reader(nullptr);
          const uint8_t* buf;
          size_t sz;

          EXPECT_TRUE(reader.PopArray(&array_reader));

          for (auto* uuid_in_bytes : kTestUuidInBytes) {
            EXPECT_TRUE(array_reader.PopArrayOfBytes(&buf, &sz));
            EXPECT_EQ(sz, kUUIDSize);
            EXPECT_EQ(std::vector<uint8_t>(uuid_in_bytes, uuid_in_bytes + sz),
                      std::vector<uint8_t>(buf, buf + sz));
          }
          EXPECT_FALSE(reader.HasMoreData());
          EXPECT_FALSE(array_reader.HasMoreData());

          // Create a fake response with uint32_t return value.
          auto response = ::dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendUint32(kTestCallbackId);
          std::move(*cb).Run(response.get(), /*err=*/nullptr);
        });

    client_->SetAllowedServices(
        base::BindLambdaForTesting([](DBusResult<Void> ret) {}), kTestUuidStr);
  }

  int adapter_index_ = 5;
  dbus::ObjectPath admin_path_;
  dbus::ObjectPath callback_path_;

  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockObjectProxy> object_proxy_;
  std::unique_ptr<FlossAdminClient> client_;

  // For observer test inspections.
  std::optional<std::tuple<FlossDeviceId, std::optional<PolicyEffect>>>
      fake_device_policy_effect_info_;
  std::vector<device::BluetoothUUID> fake_service_allowlist_info_;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossAdminClientTest> weak_ptr_factory_{this};
};

TEST_F(FlossAdminClientTest, TestInitExportRegisterAdmin) {
  TestInit();
}

TEST_F(FlossAdminClientTest, TestSetServiceAllowlistBeforeInit) {
  TestSetServiceAllowlist();
  TestInit();
}

TEST_F(FlossAdminClientTest, TestSetServiceAllowlistAfterInit) {
  TestInit();
  TestSetServiceAllowlist();
}
}  // namespace floss
