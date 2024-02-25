// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_lescan_client.h"

#include <map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
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

const char kTestSender[] = ":0.1";
const int kTestSerial = 1;
constexpr uint8_t kTestUuidByteArray[] = {0, 1, 2,  3,  4,  5,  6,  7,
                                          8, 9, 10, 11, 12, 13, 14, 15};
constexpr char kTestUuidStr[] = "00010203-0405-0607-0809-0a0b0c0d0e0f";
const uint8_t kTestScannerId = 10;
const GattStatus kTestStatus = GattStatus::kSuccess;
const uint32_t kTestCallbackId = 1000;
constexpr char kTestDeviceName[] = "FlossDevice";
constexpr char kTestDeviceAddr[] = "11:22:33:44:55:66";
const uint8_t kTestAddrType = 2;
const uint16_t kTestEventType = 3;
const uint8_t kTestPrimaryPhy = 4;
const uint8_t kTestSecondaryPhy = 5;
const uint8_t kTestAdvSid = 6;
const int8_t kTestTxPower = 7;
const int8_t kTestRssi = 8;
const uint16_t kTestPeriodicAdvInt = 9;
const uint8_t kTestFlags = 10;
const uint16_t kTestManufacturerId = 11;
const std::vector<uint8_t> kTestAdvData = {0, 1, 2};

void FakeExportMethod(
    const std::string& interface_name,
    const std::string& method_name,
    const dbus::ExportedObject::MethodCallCallback& method_call_callback,
    dbus::ExportedObject::OnExportedCallback on_exported_callback) {
  std::move(on_exported_callback)
      .Run(interface_name, method_name, /*success=*/true);
}

}  // namespace

class FlossLEScanClientTest : public testing::Test,
                              public ScannerClientObserver {
 public:
  FlossLEScanClientTest() = default;

  base::Version GetCurrVersion() {
    return floss::version::GetMaximalSupportedVersion();
  }

  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(options);
    client_ = FlossLEScanClient::Create();
    client_->AddObserver(this);

    gatt_path_ = FlossDBusClient::GenerateGattPath(adapter_index_);
    callback_path_ = dbus::ObjectPath(kScannerCallbackPath);

    object_proxy_ = base::MakeRefCounted<::dbus::MockObjectProxy>(
        bus_.get(), kAdapterInterface, gatt_path_);

    EXPECT_CALL(*bus_.get(), GetObjectProxy(kAdapterInterface, gatt_path_))
        .WillRepeatedly(::testing::Return(object_proxy_.get()));
  }

  void TearDown() override {
    // Clean up the client first so it gets rid of all its references to the
    // various buses, object proxies, etc.
    client_.reset();
  }

  // ScannerClientObserver overrides
  void ScannerRegistered(device::BluetoothUUID uuid,
                         uint8_t scanner_id,
                         GattStatus status) override {
    fake_scanner_registered_info_ = {uuid, scanner_id, status};
  }

  void ScanResultReceived(ScanResult scan_result) override {
    fake_scan_result_ = scan_result;
  }

  void WriteScanResult(dbus::MessageWriter* writer, ScanResult* scan_result) {
    dbus::MessageWriter array(nullptr);

    writer->OpenArray("{sv}", &array);

    FlossDBusClient::WriteDictEntry(&array, "name", scan_result->name);
    FlossDBusClient::WriteDictEntry(&array, "address", scan_result->address);
    FlossDBusClient::WriteDictEntry(&array, "addr_type",
                                    scan_result->addr_type);
    FlossDBusClient::WriteDictEntry(&array, "event_type",
                                    scan_result->event_type);
    FlossDBusClient::WriteDictEntry(&array, "primary_phy",
                                    scan_result->primary_phy);
    FlossDBusClient::WriteDictEntry(&array, "secondary_phy",
                                    scan_result->secondary_phy);
    FlossDBusClient::WriteDictEntry(&array, "advertising_sid",
                                    scan_result->advertising_sid);
    FlossDBusClient::WriteDictEntry(&array, "tx_power", scan_result->tx_power);
    FlossDBusClient::WriteDictEntry(&array, "rssi", scan_result->rssi);
    FlossDBusClient::WriteDictEntry(&array, "periodic_adv_int",
                                    scan_result->periodic_adv_int);
    FlossDBusClient::WriteDictEntry(&array, "flags", scan_result->flags);
    FlossDBusClient::WriteDictEntry(&array, "service_uuids",
                                    scan_result->service_uuids);
    FlossDBusClient::WriteDictEntry(&array, "service_data",
                                    scan_result->service_data);
    FlossDBusClient::WriteDictEntry(&array, "manufacturer_data",
                                    scan_result->manufacturer_data);
    FlossDBusClient::WriteDictEntry(&array, "adv_data", scan_result->adv_data);

    writer->CloseContainer(&array);
  }

  void TestOnScannerRegistered(
      dbus::ExportedObject::MethodCallCallback method_handler) {
    dbus::MethodCall method_call("some.interface",
                                 adapter::kOnScannerRegistered);
    method_call.SetPath(callback_path_);
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfBytes(kTestUuidByteArray);
    writer.AppendByte(kTestScannerId);
    writer.AppendUint32(static_cast<uint32_t>(kTestStatus));

    std::unique_ptr<dbus::Response> saved_response;
    method_handler.Run(&method_call,
                       base::BindOnce(
                           [](std::unique_ptr<dbus::Response>* saved_response,
                              std::unique_ptr<dbus::Response> response) {
                             *saved_response = std::move(response);
                           },
                           &saved_response));

    ASSERT_TRUE(!!saved_response);
    EXPECT_EQ("", saved_response->GetErrorName());

    EXPECT_EQ(std::make_tuple(device::BluetoothUUID(kTestUuidStr),
                              kTestScannerId, kTestStatus),
              fake_scanner_registered_info_);
  }

  void TestOnScanResult(
      dbus::ExportedObject::MethodCallCallback method_handler) {
    dbus::MethodCall method_call("some.interface", adapter::kOnScanResult);
    method_call.SetPath(callback_path_);
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);
    ScanResult scan_result;
    scan_result.name = kTestDeviceName;
    scan_result.address = kTestDeviceAddr;
    scan_result.addr_type = kTestAddrType;
    scan_result.event_type = kTestEventType;
    scan_result.primary_phy = kTestPrimaryPhy;
    scan_result.secondary_phy = kTestSecondaryPhy;
    scan_result.advertising_sid = kTestAdvSid;
    scan_result.tx_power = kTestTxPower;
    scan_result.rssi = kTestRssi;
    scan_result.periodic_adv_int = kTestPeriodicAdvInt;
    scan_result.flags = kTestFlags;
    scan_result.service_uuids = std::vector<device::BluetoothUUID>(
        {device::BluetoothUUID(kTestUuidStr)});
    scan_result.service_data = std::map<std::string, std::vector<uint8_t>>(
        {{kTestUuidStr, kTestAdvData}});
    scan_result.manufacturer_data = std::map<uint16_t, std::vector<uint8_t>>(
        {{kTestManufacturerId, kTestAdvData}});
    scan_result.adv_data = kTestAdvData;
    WriteScanResult(&writer, &scan_result);

    std::unique_ptr<dbus::Response> saved_response;
    method_handler.Run(&method_call,
                       base::BindOnce(
                           [](std::unique_ptr<dbus::Response>* saved_response,
                              std::unique_ptr<dbus::Response> response) {
                             *saved_response = std::move(response);
                           },
                           &saved_response));

    ASSERT_TRUE(!!saved_response);
    EXPECT_EQ("", saved_response->GetErrorName());

    EXPECT_EQ(fake_scan_result_.name, kTestDeviceName);
    EXPECT_EQ(fake_scan_result_.address, kTestDeviceAddr);
    EXPECT_EQ(fake_scan_result_.addr_type, kTestAddrType);
    EXPECT_EQ(fake_scan_result_.event_type, kTestEventType);
    EXPECT_EQ(fake_scan_result_.primary_phy, kTestPrimaryPhy);
    EXPECT_EQ(fake_scan_result_.secondary_phy, kTestSecondaryPhy);
    EXPECT_EQ(fake_scan_result_.advertising_sid, kTestAdvSid);
    EXPECT_EQ(fake_scan_result_.tx_power, kTestTxPower);
    EXPECT_EQ(fake_scan_result_.rssi, kTestRssi);
    EXPECT_EQ(fake_scan_result_.periodic_adv_int, kTestPeriodicAdvInt);
    EXPECT_EQ(fake_scan_result_.flags, kTestFlags);
    EXPECT_EQ(fake_scan_result_.service_uuids.size(), 1UL);
    EXPECT_EQ(base::ranges::count(fake_scan_result_.service_uuids,
                                  device::BluetoothUUID(kTestUuidStr)),
              1);
    EXPECT_EQ(fake_scan_result_.service_data.size(), 1UL);
    EXPECT_EQ(fake_scan_result_.service_data[kTestUuidStr], kTestAdvData);
    EXPECT_EQ(fake_scan_result_.manufacturer_data.size(), 1UL);
    EXPECT_EQ(fake_scan_result_.manufacturer_data[kTestManufacturerId],
              kTestAdvData);
    EXPECT_EQ(fake_scan_result_.adv_data, kTestAdvData);
  }

  int adapter_index_ = 5;
  dbus::ObjectPath gatt_path_;
  dbus::ObjectPath callback_path_;

  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockObjectProxy> object_proxy_;
  std::unique_ptr<FlossLEScanClient> client_;

  // For observer test inspections.
  std::optional<std::tuple<device::BluetoothUUID, uint8_t, GattStatus>>
      fake_scanner_registered_info_;
  ScanResult fake_scan_result_;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossLEScanClientTest> weak_ptr_factory_{this};
};

static bool ReadNullOptDBusParam(dbus::MessageReader* reader) {
  std::optional<int32_t> param;
  if (!FlossDBusClient::ReadDBusParam(reader, &param)) {
    return false;
  }
  return param == std::nullopt;
}

TEST_F(FlossLEScanClientTest, TestInitExportRegisterScanner) {
  scoped_refptr<::dbus::MockExportedObject> exported_callback =
      base::MakeRefCounted<::dbus::MockExportedObject>(bus_.get(),
                                                       callback_path_);

  // Expected exported callbacks
  dbus::ExportedObject::MethodCallCallback method_handler_on_scanner_registered;
  EXPECT_CALL(
      *exported_callback.get(),
      ExportMethod(kScannerCallbackInterfaceName, adapter::kOnScannerRegistered,
                   testing::_, testing::_))
      .WillOnce(
          DoAll(testing::SaveArg<2>(&method_handler_on_scanner_registered),
                &FakeExportMethod));

  dbus::ExportedObject::MethodCallCallback method_handler_on_scan_result;
  EXPECT_CALL(*exported_callback.get(),
              ExportMethod(kScannerCallbackInterfaceName,
                           adapter::kOnScanResult, testing::_, testing::_))
      .WillOnce(DoAll(testing::SaveArg<2>(&method_handler_on_scan_result),
                      &FakeExportMethod));

  dbus::ExportedObject::MethodCallCallback method_handler_on_adv_found;
  EXPECT_CALL(
      *exported_callback.get(),
      ExportMethod(kScannerCallbackInterfaceName,
                   adapter::kOnAdvertisementFound, testing::_, testing::_))
      .WillOnce(DoAll(testing::SaveArg<2>(&method_handler_on_adv_found),
                      &FakeExportMethod));

  dbus::ExportedObject::MethodCallCallback method_handler_on_adv_lost;
  EXPECT_CALL(
      *exported_callback.get(),
      ExportMethod(kScannerCallbackInterfaceName, adapter::kOnAdvertisementLost,
                   testing::_, testing::_))
      .WillOnce(DoAll(testing::SaveArg<2>(&method_handler_on_adv_lost),
                      &FakeExportMethod));

  EXPECT_CALL(*bus_.get(), GetExportedObject(callback_path_))
      .WillRepeatedly(testing::Return(exported_callback.get()));

  // Expected call to RegisterScannerCallback when client is initialized
  EXPECT_CALL(*object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kRegisterScannerCallback), _, _))
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

  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  // Test exported callbacks are correctly parsed
  ASSERT_TRUE(!!method_handler_on_scanner_registered);
  ASSERT_TRUE(!!method_handler_on_scan_result);
  ASSERT_TRUE(!!method_handler_on_adv_found);
  ASSERT_TRUE(!!method_handler_on_adv_lost);

  TestOnScannerRegistered(method_handler_on_scanner_registered);
  TestOnScanResult(method_handler_on_scan_result);

  // Test RegisterScanner
  EXPECT_CALL(*object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kRegisterScanner), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint32_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(kTestCallbackId, param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with UUID return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendArrayOfBytes(kTestUuidByteArray);
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  client_->RegisterScanner(
      base::BindLambdaForTesting([](DBusResult<device::BluetoothUUID> ret) {
        EXPECT_TRUE(ret.has_value());
        EXPECT_EQ(device::BluetoothUUID(kTestUuidStr), *ret);
      }));

  // Test UnregisterScanner
  EXPECT_CALL(*object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kUnregisterScanner), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint8_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(kTestScannerId, param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with bool return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  client_->UnregisterScanner(
      base::BindLambdaForTesting([](DBusResult<bool> ret) {
        // Check that there is no error and return is parsed correctly
        EXPECT_TRUE(ret.has_value());
        EXPECT_EQ(true, *ret);
      }),
      kTestScannerId);

  // Expected UnregisterScannerCallback once client is cleaned up
  EXPECT_CALL(*object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kUnregisterScannerCallback), _, _))
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

TEST_F(FlossLEScanClientTest, TestStartStopScan) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  // Method of 3 parameters with no return.
  EXPECT_CALL(*object_proxy_.get(), DoCallMethodWithErrorResponse(
                                        HasMemberOf(adapter::kStartScan), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        uint8_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadDBusParam(&msg, &param1));
        EXPECT_EQ(kTestScannerId, param1);
        ASSERT_TRUE(ReadNullOptDBusParam(&msg));  // ScanSettings
        ASSERT_TRUE(ReadNullOptDBusParam(&msg));  // ScanFilter

        // Create a fake response with BtifStatus return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendUint32(
            static_cast<uint32_t>(FlossDBusClient::BtifStatus::kSuccess));
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  client_->StartScan(base::BindLambdaForTesting(
                         [](DBusResult<FlossDBusClient::BtifStatus> ret) {
                           // Check that there is no error and return is parsed
                           // correctly
                           EXPECT_TRUE(ret.has_value());
                           EXPECT_EQ(ret.value(),
                                     FlossDBusClient::BtifStatus::kSuccess);
                         }),
                     kTestScannerId, std::nullopt /* ScanSettings */,
                     std::nullopt /* ScanFilter*/);

  // Method of 1 parameter with no return.
  EXPECT_CALL(*object_proxy_.get(), DoCallMethodWithErrorResponse(
                                        HasMemberOf(adapter::kStopScan), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint8_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(kTestScannerId, param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with BtifStatus return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendUint32(
            static_cast<uint32_t>(FlossDBusClient::BtifStatus::kSuccess));
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  client_->StopScan(base::BindLambdaForTesting(
                        [](DBusResult<FlossDBusClient::BtifStatus> ret) {
                          // Check that there is no error and return is parsed
                          // correctly
                          EXPECT_TRUE(ret.has_value());
                          EXPECT_EQ(ret.value(),
                                    FlossDBusClient::BtifStatus::kSuccess);
                        }),
                    kTestScannerId);
}

}  // namespace floss
