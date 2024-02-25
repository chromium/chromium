// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_advertiser_client.h"

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
#include "device/bluetooth/floss/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::DoAll;

constexpr char kTestSender[] = ":0.1";
const int kTestSerial = 1;
const int32_t kRegId1 = 1;
const int32_t kAdvId1 = 2;
const uint32_t kCallbackId1 = 3;

void FakeExportMethod(
    const std::string& interface_name,
    const std::string& method_name,
    const dbus::ExportedObject::MethodCallCallback& method_call_callback,
    dbus::ExportedObject::OnExportedCallback on_exported_callback) {
  std::move(on_exported_callback)
      .Run(interface_name, method_name, /*success=*/true);
}

}  // namespace

namespace floss {

class FlossAdvertiserClientTest : public testing::Test,
                                  public FlossAdvertiserClientObserver {
 public:
  FlossAdvertiserClientTest() = default;

  base::Version GetCurrVersion() {
    return floss::version::GetMaximalSupportedVersion();
  }

  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(options);
    advclient_ = FlossAdvertiserClient::Create();
    callback_path_ = ::dbus::ObjectPath(kAdvertisingSetCallbackPath);

    gatt_adapter_path_ = FlossDBusClient::GenerateGattPath(adapter_index_);
    advclient_proxy_ = base::MakeRefCounted<::dbus::MockObjectProxy>(
        bus_.get(), kGattInterface, gatt_adapter_path_);
  }

  void TearDown() override {
    // Clean up the advertiser client first to get rid of all references to
    // various buses, object proxies, etc.
    advclient_.reset();
  }

  void DoOnAdvertisingSetStarted(
      dbus::ExportedObject::MethodCallCallback method_handler,
      RegId reg_id,
      AdvertiserId adv_id,
      int32_t tx_power,
      AdvertisingStatus status) {
    dbus::MethodCall method_call("some.interface",
                                 advertiser::kOnAdvertisingSetStarted);

    method_call.SetPath(callback_path_);
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(reg_id);
    writer.AppendInt32(adv_id);
    writer.AppendInt32(tx_power);
    writer.AppendUint32(static_cast<uint32_t>(status));

    method_handler.Run(
        &method_call,
        base::BindOnce([](std::unique_ptr<dbus::Response> response) {
          ASSERT_TRUE(!!response);
          EXPECT_NE(response->GetMessageType(),
                    dbus::Message::MessageType::MESSAGE_ERROR);
        }));
  }

  void DoOnAdvertisingSetStopped(
      dbus::ExportedObject::MethodCallCallback method_handler,
      AdvertiserId adv_id) {
    dbus::MethodCall method_call("some.interface",
                                 advertiser::kOnAdvertisingSetStopped);

    method_call.SetPath(callback_path_);
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(adv_id);

    method_handler.Run(
        &method_call,
        base::BindOnce([](std::unique_ptr<dbus::Response> response) {
          ASSERT_TRUE(!!response);
          EXPECT_NE(response->GetMessageType(),
                    dbus::Message::MessageType::MESSAGE_ERROR);
        }));
  }

  void DoOnAdvertisingParametersUpdated(
      dbus::ExportedObject::MethodCallCallback method_handler,
      AdvertiserId adv_id,
      int32_t tx_power,
      AdvertisingStatus status) {
    dbus::MethodCall method_call("some.interface",
                                 advertiser::kOnAdvertisingParametersUpdated);

    method_call.SetPath(callback_path_);
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(adv_id);
    writer.AppendInt32(tx_power);
    writer.AppendUint32(static_cast<uint32_t>(status));

    method_handler.Run(
        &method_call,
        base::BindOnce([](std::unique_ptr<dbus::Response> response) {
          ASSERT_TRUE(!!response);
          EXPECT_NE(response->GetMessageType(),
                    dbus::Message::MessageType::MESSAGE_ERROR);
        }));
  }

  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockExportedObject> exported_callbacks_;
  scoped_refptr<::dbus::MockObjectProxy> advclient_proxy_;
  std::unique_ptr<FlossAdvertiserClient> advclient_;
  dbus::ObjectPath callback_path_;
  dbus::ObjectPath gatt_adapter_path_;
  int adapter_index_ = 2;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossAdvertiserClientTest> weak_ptr_factory_{this};
};

TEST_F(FlossAdvertiserClientTest, StartAndStopAdvertisingSet) {
  exported_callbacks_ = base::MakeRefCounted<::dbus::MockExportedObject>(
      bus_.get(), callback_path_);

  EXPECT_CALL(*bus_.get(), GetExportedObject)
      .WillRepeatedly(::testing::Return(exported_callbacks_.get()));

  EXPECT_CALL(*bus_.get(), GetObjectProxy(kGattInterface, gatt_adapter_path_))
      .WillRepeatedly(::testing::Return(advclient_proxy_.get()));

  // There are 7 exported methods that we don't interested in it.
  EXPECT_CALL(*exported_callbacks_.get(), ExportMethod)
      .Times(7)
      .WillRepeatedly(&FakeExportMethod);

  // Handle OnAdvertisingSetStarted, OnAdvertisingParametersUpdated,
  // OnAdvertisingSetStopped
  dbus::ExportedObject::MethodCallCallback
      method_handler_on_advertising_set_started;
  dbus::ExportedObject::MethodCallCallback
      method_handler_on_advertising_parameters_updated;
  dbus::ExportedObject::MethodCallCallback
      method_handler_on_advertising_set_stopped;

  EXPECT_CALL(*exported_callbacks_.get(),
              ExportMethod(advertiser::kCallbackInterface,
                           advertiser::kOnAdvertisingSetStarted, _, _))
      .WillOnce(
          DoAll(testing::SaveArg<2>(&method_handler_on_advertising_set_started),
                &FakeExportMethod));

  EXPECT_CALL(*exported_callbacks_.get(),
              ExportMethod(advertiser::kCallbackInterface,
                           advertiser::kOnAdvertisingParametersUpdated, _, _))
      .WillOnce(DoAll(testing::SaveArg<2>(
                          &method_handler_on_advertising_parameters_updated),
                      &FakeExportMethod));

  EXPECT_CALL(*exported_callbacks_.get(),
              ExportMethod(advertiser::kCallbackInterface,
                           advertiser::kOnAdvertisingSetStopped, _, _))
      .WillOnce(
          DoAll(testing::SaveArg<2>(&method_handler_on_advertising_set_stopped),
                &FakeExportMethod));

  EXPECT_CALL(*advclient_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(advertiser::kRegisterCallback), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        auto response = ::dbus::Response::CreateEmpty();
        ::dbus::MessageWriter msg(response.get());
        FlossDBusClient::WriteAllDBusParams(&msg, kCallbackId1);
        std::move(*cb).Run(response.get(), nullptr);
      });

  advclient_->Init(bus_.get(), kGattInterface, adapter_index_, GetCurrVersion(),
                   base::DoNothing());
  ASSERT_TRUE(!!method_handler_on_advertising_set_started);
  ASSERT_TRUE(!!method_handler_on_advertising_parameters_updated);
  ASSERT_TRUE(!!method_handler_on_advertising_set_stopped);

  // Do StartAdvertisingSet
  EXPECT_CALL(*advclient_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(advertiser::kStartAdvertisingSet), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        auto response = ::dbus::Response::CreateEmpty();
        ::dbus::MessageWriter msg(response.get());
        FlossDBusClient::WriteAllDBusParams(&msg,
                                            static_cast<int32_t>(kRegId1));
        std::move(*cb).Run(response.get(), nullptr);
      });

  AdvertisingSetParameters params = {};
  AdvertiseData adv_data = {};
  adv_data.include_tx_power_level = false;
  adv_data.include_device_name = false;

  base::RunLoop run_loop0;
  advclient_->StartAdvertisingSet(
      params, adv_data, /*scan_rsp=*/std::nullopt,
      /*periodic_params=*/std::nullopt, /*periodic_data=*/std::nullopt,
      /*duration=*/1,
      /*max_ext_adv_events=*/1,
      base::BindLambdaForTesting([&run_loop0](AdvertiserId adv_id) {
        EXPECT_EQ(kAdvId1, adv_id);
        run_loop0.Quit();
      }),
      base::BindOnce([](device::BluetoothAdvertisement::ErrorCode error_code) {
        FAIL();
      }));
  DoOnAdvertisingSetStarted(method_handler_on_advertising_set_started, kRegId1,
                            kAdvId1, /*tx_power=*/0,
                            AdvertisingStatus::kSuccess);
  run_loop0.Run();

  // Do SetAdvertisingParameters
  EXPECT_CALL(*advclient_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(advertiser::kSetAdvertisingParameters), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        auto response = ::dbus::Response::CreateEmpty();
        std::move(*cb).Run(response.get(), nullptr);
      });

  base::RunLoop run_loop1;
  advclient_->SetAdvertisingParameters(
      kAdvId1, params,
      base::BindLambdaForTesting([&run_loop1]() { run_loop1.Quit(); }),
      base::BindOnce([](device::BluetoothAdvertisement::ErrorCode error_code) {
        FAIL();
      }));
  DoOnAdvertisingParametersUpdated(
      method_handler_on_advertising_parameters_updated, kAdvId1, 0,
      AdvertisingStatus::kSuccess);
  run_loop1.Run();

  // Do StopAdvertisingSet
  EXPECT_CALL(*advclient_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(advertiser::kStopAdvertisingSet), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        auto response = ::dbus::Response::CreateEmpty();
        std::move(*cb).Run(response.get(), nullptr);
      });

  base::RunLoop run_loop2;
  advclient_->StopAdvertisingSet(
      kAdvId1, base::BindLambdaForTesting([&run_loop2]() { run_loop2.Quit(); }),
      base::BindOnce([](device::BluetoothAdvertisement::ErrorCode error_code) {
        FAIL();
      }));

  DoOnAdvertisingSetStopped(method_handler_on_advertising_set_stopped, kAdvId1);
  run_loop2.Run();

  // Expected call to UnregisterCallback when client is destroyed
  EXPECT_CALL(*advclient_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(advertiser::kUnregisterCallback), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint32_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(kCallbackId1, param1);
        EXPECT_FALSE(msg.HasMoreData());
      });
}

}  // namespace floss
