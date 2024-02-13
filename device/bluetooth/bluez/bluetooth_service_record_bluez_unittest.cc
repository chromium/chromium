// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bluez {

namespace {

constexpr uint16_t kServiceUuidAttributeId = 0x0003;

constexpr char kServiceUuid1[] = "00001801-0000-1000-8000-00805f9b34fb";
constexpr char kServiceUuid2[] = "00001801-0000-1000-7000-00805f9b34fb";
constexpr char kServiceUuid3[] = "00001801-0000-1000-3000-00805f9b34fb";

}  // namespace

class BluetoothServiceRecordBlueZTest : public device::BluetoothTestBlueZ {
 public:
  BluetoothServiceRecordBlueZTest()
      : adapter_bluez_(nullptr),
        success_callbacks_(0),
        error_callbacks_(0),
        last_seen_handle_(0) {}

  BluetoothServiceRecordBlueZTest(const BluetoothServiceRecordBlueZTest&) =
      delete;
  BluetoothServiceRecordBlueZTest& operator=(
      const BluetoothServiceRecordBlueZTest&) = delete;

  void SetUp() override {
    BluetoothTestBlueZ::SetUp();
    InitWithFakeAdapter();
    adapter_bluez_ = static_cast<bluez::BluetoothAdapterBlueZ*>(adapter_.get());
  }

  uint32_t CreateServiceRecordWithCallbacks(
      const BluetoothServiceRecordBlueZ& record) {
    const size_t old_success_callbacks = success_callbacks_;
    const size_t old_error_callbacks = error_callbacks_;
    // Note: Our fake implementation will never give us a handle of 0, so we
    // can know that we will never have a record with the handle 0. Hence using
    // 0 as an invalid handle return value is okay.
    last_seen_handle_ = 0;
    adapter_bluez_->CreateServiceRecord(
        record,
        base::BindOnce(
            &BluetoothServiceRecordBlueZTest::CreateServiceSuccessCallback,
            base::Unretained(this)),
        base::BindOnce(&BluetoothServiceRecordBlueZTest::ErrorCallback,
                       base::Unretained(this)));
    EXPECT_EQ(old_success_callbacks + 1, success_callbacks_);
    EXPECT_EQ(old_error_callbacks, error_callbacks_);
    EXPECT_NE(0u, last_seen_handle_);
    return last_seen_handle_;
  }

  void RemoveServiceRecordWithCallbacks(uint32_t handle, bool expect_success) {
    const size_t old_success_callbacks = success_callbacks_;
    const size_t old_error_callbacks = error_callbacks_;
    adapter_bluez_->RemoveServiceRecord(
        handle,
        base::BindOnce(
            &BluetoothServiceRecordBlueZTest::RemoveServiceSuccessCallback,
            base::Unretained(this)),
        base::BindOnce(&BluetoothServiceRecordBlueZTest::ErrorCallback,
                       base::Unretained(this)));
    size_t success = expect_success ? 1 : 0;
    EXPECT_EQ(old_success_callbacks + success, success_callbacks_);
    EXPECT_EQ(old_error_callbacks + 1 - success, error_callbacks_);
  }

  void GetServiceRecords(BluetoothDeviceBlueZ* device, bool expect_success) {
    const size_t old_success_callbacks = success_callbacks_;
    const size_t old_error_callbacks = error_callbacks_;
    records_.clear();
    device->GetServiceRecords(
        base::BindOnce(
            &BluetoothServiceRecordBlueZTest::GetServiceRecordsCallback,
            base::Unretained(this)),
        base::BindOnce(&BluetoothServiceRecordBlueZTest::ErrorCallback,
                       base::Unretained(this)));
    size_t success = expect_success ? 1 : 0;
    EXPECT_EQ(old_success_callbacks + success, success_callbacks_);
    EXPECT_EQ(old_error_callbacks + 1 - success, error_callbacks_);
  }

  void VerifyRecords() {
    EXPECT_EQ(2u, records_.size());

    std::vector<uint16_t> ids0 = records_[0].GetAttributeIds();
    EXPECT_EQ(2u, ids0.size());

    BluetoothServiceAttributeValueBlueZ service_handle0 =
        records_[0].GetAttributeValue(ids0[0]);
    ASSERT_TRUE(service_handle0.value().is_int());
    EXPECT_EQ(0x1337, service_handle0.value().GetInt());

    BluetoothServiceAttributeValueBlueZ service_class_list =
        records_[0].GetAttributeValue(ids0[1]);
    const base::Value& value = service_class_list.sequence()[0].value();
    EXPECT_TRUE(value.is_string());
    EXPECT_EQ("1802", value.GetString());

    std::vector<uint16_t> ids1 = records_[1].GetAttributeIds();
    EXPECT_EQ(1u, ids1.size());

    BluetoothServiceAttributeValueBlueZ service_handle1 =
        records_[1].GetAttributeValue(ids1[0]);
    ASSERT_TRUE(service_handle1.value().is_int());
    EXPECT_EQ(0xffffffff,
              static_cast<uint32_t>(service_handle1.value().GetInt()));
  }

 protected:
  BluetoothServiceRecordBlueZ CreateaServiceRecord(const std::string uuid) {
    BluetoothServiceRecordBlueZ record;
    record.AddRecordEntry(
        kServiceUuidAttributeId,
        BluetoothServiceAttributeValueBlueZ(
            BluetoothServiceAttributeValueBlueZ::UUID, 16, base::Value(uuid)));
    return record;
  }

  raw_ptr<BluetoothAdapterBlueZ, DanglingUntriaged> adapter_bluez_;
  size_t success_callbacks_;
  size_t error_callbacks_;

 private:
  void CreateServiceSuccessCallback(uint32_t handle) {
    last_seen_handle_ = handle;
    ++success_callbacks_;
  }

  void RemoveServiceSuccessCallback() { ++success_callbacks_; }

  void ErrorCallback(BluetoothServiceRecordBlueZ::ErrorCode code) {
    ++error_callbacks_;
  }

  void GetServiceRecordsCallback(
      const std::vector<BluetoothServiceRecordBlueZ>& records) {
    records_ = records;
    ++success_callbacks_;
  }

  uint32_t last_seen_handle_;
  std::vector<BluetoothServiceRecordBlueZ> records_;
};

TEST_F(BluetoothServiceRecordBlueZTest, CreateAndRemove) {
  uint32_t handle1 =
      CreateServiceRecordWithCallbacks(CreateaServiceRecord(kServiceUuid1));
  uint32_t handle2 =
      CreateServiceRecordWithCallbacks(CreateaServiceRecord(kServiceUuid2));
  uint32_t handle3 =
      CreateServiceRecordWithCallbacks(CreateaServiceRecord(kServiceUuid1));
  uint32_t handle4 =
      CreateServiceRecordWithCallbacks(CreateaServiceRecord(kServiceUuid3));

  RemoveServiceRecordWithCallbacks(handle1, true);
  RemoveServiceRecordWithCallbacks(handle3, true);
  RemoveServiceRecordWithCallbacks(handle1, false);
  RemoveServiceRecordWithCallbacks(handle4, true);
  RemoveServiceRecordWithCallbacks(handle2, true);
}

TEST_F(BluetoothServiceRecordBlueZTest, GetServiceRecords) {
  BluetoothDeviceBlueZ* device =
      static_cast<BluetoothDeviceBlueZ*>(adapter_->GetDevice(
          bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress));
  GetServiceRecords(device, false);
  base::RunLoop run_loop;
  device->Connect(
      nullptr,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<device::BluetoothDevice::ConnectErrorCode>
                          error_code) {
            EXPECT_FALSE(error_code.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();
  GetServiceRecords(device, true);
  VerifyRecords();
}

}  // namespace bluez
