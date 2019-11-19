// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test.h"

#include <iterator>
#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_common.h"

namespace device {

BluetoothTestBase::LowEnergyDeviceData::LowEnergyDeviceData() = default;

BluetoothTestBase::LowEnergyDeviceData::LowEnergyDeviceData(
    LowEnergyDeviceData&& data) = default;

BluetoothTestBase::LowEnergyDeviceData::~LowEnergyDeviceData() = default;

const char BluetoothTestBase::kTestAdapterName[] = "FakeBluetoothAdapter";
const char BluetoothTestBase::kTestAdapterAddress[] = "A1:B2:C3:D4:E5:F6";

const char BluetoothTestBase::kTestDeviceName[] = "FakeBluetoothDevice";
const char BluetoothTestBase::kTestDeviceNameEmpty[] = "";
const char BluetoothTestBase::kTestDeviceNameU2f[] = "U2F FakeDevice";
const char BluetoothTestBase::kTestDeviceNameCable[] = "Cable FakeDevice";

const char BluetoothTestBase::kTestDeviceAddress1[] = "01:00:00:90:1E:BE";
const char BluetoothTestBase::kTestDeviceAddress2[] = "02:00:00:8B:74:63";
const char BluetoothTestBase::kTestDeviceAddress3[] = "03:00:00:17:C0:57";

// Service UUIDs
const char BluetoothTestBase::kTestUUIDGenericAccess[] =
    "00001800-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDGenericAttribute[] =
    "00001801-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDImmediateAlert[] =
    "00001802-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDLinkLoss[] =
    "00001803-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDHeartRate[] =
    "0000180d-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDU2f[] =
    "0000fffd-0000-1000-8000-00805f9b34fb";
// Characteristic UUIDs
const char BluetoothTestBase::kTestUUIDDeviceName[] =
    "00002a00-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDAppearance[] =
    "00002a01-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDReconnectionAddress[] =
    "00002a03-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDHeartRateMeasurement[] =
    "00002a37-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDU2fControlPointLength[] =
    "f1d0fff3-deaa-ecee-b42f-c9ba7ed623bb";
// Descriptor UUIDs
const char BluetoothTestBase::kTestUUIDCharacteristicUserDescription[] =
    "00002901-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDClientCharacteristicConfiguration[] =
    "00002902-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDServerCharacteristicConfiguration[] =
    "00002903-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDCharacteristicPresentationFormat[] =
    "00002904-0000-1000-8000-00805f9b34fb";
const char BluetoothTestBase::kTestUUIDCableAdvertisement[] =
    "0000fde2-0000-1000-8000-00805f9b34fb";
// Manufacturer kTestAdapterAddress
const uint16_t BluetoothTestBase::kTestManufacturerId = 0x00E0;
const uint8_t BluetoothTestBase::kTestCableEid[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15};
const char BluetoothTestBase::kTestUuidFormattedClientEid[] =
    "00010203-0405-0607-0809-101112131415";

BluetoothTestBase::BluetoothTestBase() {}

BluetoothTestBase::~BluetoothTestBase() = default;
void BluetoothTestBase::StartLowEnergyDiscoverySession() {
  adapter_->StartDiscoverySessionWithFilter(
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE),
      GetDiscoverySessionCallback(Call::EXPECTED),
      GetErrorCallback(Call::NOT_EXPECTED));
  base::RunLoop().RunUntilIdle();
}

void BluetoothTestBase::StartLowEnergyDiscoverySessionExpectedToFail() {
  adapter_->StartDiscoverySessionWithFilter(
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE),
      GetDiscoverySessionCallback(Call::NOT_EXPECTED),
      GetErrorCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();
}

void BluetoothTestBase::TearDown() {
  EXPECT_EQ(expected_success_callback_calls_, actual_success_callback_calls_);
  EXPECT_EQ(expected_error_callback_calls_, actual_error_callback_calls_);
  EXPECT_FALSE(unexpected_success_callback_);
  EXPECT_FALSE(unexpected_error_callback_);
}

bool BluetoothTestBase::DenyPermission() {
  return false;
}

BluetoothDevice* BluetoothTestBase::SimulateLowEnergyDevice(
    int device_ordinal) {
  NOTIMPLEMENTED();
  return nullptr;
}

BluetoothDevice* BluetoothTestBase::SimulateClassicDevice() {
  NOTIMPLEMENTED();
  return nullptr;
}

void BluetoothTestBase::SimulateDeviceBreaksConnection(
    BluetoothDevice* device) {
  SimulateGattDisconnection(device);
}

bool BluetoothTestBase::SimulateLocalGattCharacteristicNotificationsRequest(
    BluetoothLocalGattCharacteristic* characteristic,
    bool start) {
  NOTIMPLEMENTED();
  return false;
}

std::vector<uint8_t> BluetoothTestBase::LastNotifactionValueForCharacteristic(
    BluetoothLocalGattCharacteristic* characteristic) {
  NOTIMPLEMENTED();
  return std::vector<uint8_t>();
}

void BluetoothTestBase::ExpectedChangeNotifyValueAttempts(int attempts) {
  EXPECT_EQ(attempts, gatt_write_descriptor_attempts_);
  EXPECT_EQ(attempts, gatt_notify_characteristic_attempts_);
}

void BluetoothTestBase::ExpectedNotifyValue(
    NotifyValueState expected_value_state) {
  ASSERT_EQ(2u, last_write_value_.size());
  switch (expected_value_state) {
    case NotifyValueState::NONE:
      EXPECT_EQ(0, last_write_value_[0]);
      EXPECT_EQ(0, last_write_value_[1]);
      break;
    case NotifyValueState::NOTIFY:
      EXPECT_EQ(1, last_write_value_[0]);
      EXPECT_EQ(0, last_write_value_[1]);
      break;
    case NotifyValueState::INDICATE:
      EXPECT_EQ(2, last_write_value_[0]);
      EXPECT_EQ(0, last_write_value_[1]);
      break;
  }
}

std::vector<BluetoothLocalGattService*>
BluetoothTestBase::RegisteredGattServices() {
  NOTIMPLEMENTED();
  return std::vector<BluetoothLocalGattService*>();
}

void BluetoothTestBase::DeleteDevice(BluetoothDevice* device) {
  adapter_->DeleteDeviceForTesting(device->GetAddress());
}

void BluetoothTestBase::Callback(Call expected) {
  ++callback_count_;

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::CreateAdvertisementCallback(
    Call expected,
    scoped_refptr<BluetoothAdvertisement> advertisement) {
  ++callback_count_;
  advertisements_.push_back(std::move(advertisement));

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::DiscoverySessionCallback(
    Call expected,
    std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
  ++callback_count_;
  discovery_sessions_.push_back(std::move(discovery_session));

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::GattConnectionCallback(
    Call expected,
    std::unique_ptr<BluetoothGattConnection> connection) {
  ++callback_count_;
  gatt_connections_.push_back(std::move(connection));

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::NotifyCallback(
    Call expected,
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  notify_sessions_.push_back(std::move(notify_session));

  ++callback_count_;
  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::NotifyCheckForPrecedingCalls(
    int num_of_preceding_calls,
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  EXPECT_EQ(num_of_preceding_calls, callback_count_);

  notify_sessions_.push_back(std::move(notify_session));

  ++callback_count_;
  ++actual_success_callback_calls_;
}

void BluetoothTestBase::StopNotifyCallback(Call expected) {
  ++callback_count_;

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::StopNotifyCheckForPrecedingCalls(
    int num_of_preceding_calls) {
  EXPECT_EQ(num_of_preceding_calls, callback_count_);

  ++callback_count_;
  ++actual_success_callback_calls_;
}

void BluetoothTestBase::ReadValueCallback(Call expected,
                                          const std::vector<uint8_t>& value) {
  ++callback_count_;
  last_read_value_ = value;

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::ErrorCallback(Call expected) {
  ++error_callback_count_;

  if (expected == Call::EXPECTED)
    ++actual_error_callback_calls_;
  else
    unexpected_error_callback_ = true;
}

void BluetoothTestBase::AdvertisementErrorCallback(
    Call expected,
    BluetoothAdvertisement::ErrorCode error_code) {
  ++error_callback_count_;
  last_advertisement_error_code_ = error_code;

  if (expected == Call::EXPECTED)
    ++actual_error_callback_calls_;
  else
    unexpected_error_callback_ = true;
}

void BluetoothTestBase::ConnectErrorCallback(
    Call expected,
    enum BluetoothDevice::ConnectErrorCode error_code) {
  ++error_callback_count_;
  last_connect_error_code_ = error_code;

  if (expected == Call::EXPECTED)
    ++actual_error_callback_calls_;
  else
    unexpected_error_callback_ = true;
}

void BluetoothTestBase::GattErrorCallback(
    Call expected,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  ++error_callback_count_;
  last_gatt_error_code_ = error_code;

  if (expected == Call::EXPECTED)
    ++actual_error_callback_calls_;
  else
    unexpected_error_callback_ = true;
}

void BluetoothTestBase::ReentrantStartNotifySessionSuccessCallback(
    Call expected,
    BluetoothRemoteGattCharacteristic* characteristic,
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  ++callback_count_;
  notify_sessions_.push_back(std::move(notify_session));

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;

  characteristic->StartNotifySession(GetNotifyCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
}

void BluetoothTestBase::ReentrantStartNotifySessionErrorCallback(
    Call expected,
    BluetoothRemoteGattCharacteristic* characteristic,
    bool error_in_reentrant,
    BluetoothGattService::GattErrorCode error_code) {
  ++error_callback_count_;
  last_gatt_error_code_ = error_code;

  if (expected == Call::EXPECTED)
    ++actual_error_callback_calls_;
  else
    unexpected_error_callback_ = true;

  if (error_in_reentrant) {
    SimulateGattNotifySessionStartError(
        characteristic, BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);
    characteristic->StartNotifySession(GetNotifyCallback(Call::NOT_EXPECTED),
                                       GetGattErrorCallback(Call::EXPECTED));
  } else {
    characteristic->StartNotifySession(
        GetNotifyCallback(Call::EXPECTED),
        GetGattErrorCallback(Call::NOT_EXPECTED));
  }
}

base::Closure BluetoothTestBase::GetCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::Callback, weak_factory_.GetWeakPtr(),
                    expected);
}

BluetoothAdapter::CreateAdvertisementCallback
BluetoothTestBase::GetCreateAdvertisementCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::CreateAdvertisementCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothAdapter::DiscoverySessionCallback
BluetoothTestBase::GetDiscoverySessionCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::DiscoverySessionCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothDevice::GattConnectionCallback
BluetoothTestBase::GetGattConnectionCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::GattConnectionCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothRemoteGattCharacteristic::NotifySessionCallback
BluetoothTestBase::GetNotifyCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::NotifyCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothRemoteGattCharacteristic::NotifySessionCallback
BluetoothTestBase::GetNotifyCheckForPrecedingCalls(int num_of_preceding_calls) {
  ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::NotifyCheckForPrecedingCalls,
                    weak_factory_.GetWeakPtr(), num_of_preceding_calls);
}

base::Closure BluetoothTestBase::GetStopNotifyCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::StopNotifyCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

base::Closure BluetoothTestBase::GetStopNotifyCheckForPrecedingCalls(
    int num_of_preceding_calls) {
  ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::StopNotifyCheckForPrecedingCalls,
                    weak_factory_.GetWeakPtr(), num_of_preceding_calls);
}

BluetoothRemoteGattCharacteristic::ValueCallback
BluetoothTestBase::GetReadValueCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::BindOnce(&BluetoothTestBase::ReadValueCallback,
                        weak_factory_.GetWeakPtr(), expected);
}

BluetoothAdapter::ErrorCallback BluetoothTestBase::GetErrorCallback(
    Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_error_callback_calls_;
  return base::Bind(&BluetoothTestBase::ErrorCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothAdapter::AdvertisementErrorCallback
BluetoothTestBase::GetAdvertisementErrorCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_error_callback_calls_;
  return base::Bind(&BluetoothTestBase::AdvertisementErrorCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothDevice::ConnectErrorCallback
BluetoothTestBase::GetConnectErrorCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_error_callback_calls_;
  return base::Bind(&BluetoothTestBase::ConnectErrorCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

base::Callback<void(BluetoothRemoteGattService::GattErrorCode)>
BluetoothTestBase::GetGattErrorCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_error_callback_calls_;
  return base::Bind(&BluetoothTestBase::GattErrorCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothRemoteGattCharacteristic::NotifySessionCallback
BluetoothTestBase::GetReentrantStartNotifySessionSuccessCallback(
    Call expected,
    BluetoothRemoteGattCharacteristic* characteristic) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(
      &BluetoothTestBase::ReentrantStartNotifySessionSuccessCallback,
      weak_factory_.GetWeakPtr(), expected, characteristic);
}

base::Callback<void(BluetoothGattService::GattErrorCode)>
BluetoothTestBase::GetReentrantStartNotifySessionErrorCallback(
    Call expected,
    BluetoothRemoteGattCharacteristic* characteristic,
    bool error_in_reentrant) {
  if (expected == Call::EXPECTED)
    ++expected_error_callback_calls_;
  return base::Bind(
      &BluetoothTestBase::ReentrantStartNotifySessionErrorCallback,
      weak_factory_.GetWeakPtr(), expected, characteristic, error_in_reentrant);
}

void BluetoothTestBase::ResetEventCounts() {
  last_advertisement_error_code_ =
      BluetoothAdvertisement::INVALID_ADVERTISEMENT_ERROR_CODE;
  last_connect_error_code_ = BluetoothDevice::ERROR_UNKNOWN;
  callback_count_ = 0;
  error_callback_count_ = 0;
  gatt_connection_attempts_ = 0;
  gatt_disconnection_attempts_ = 0;
  gatt_discovery_attempts_ = 0;
  gatt_notify_characteristic_attempts_ = 0;
  gatt_read_characteristic_attempts_ = 0;
  gatt_write_characteristic_attempts_ = 0;
  gatt_read_descriptor_attempts_ = 0;
  gatt_write_descriptor_attempts_ = 0;
}

void BluetoothTestBase::RemoveTimedOutDevices() {
  adapter_->RemoveTimedOutDevices();
}

BluetoothTestBase::LowEnergyDeviceData
BluetoothTestBase::GetLowEnergyDeviceData(int device_ordinal) const {
  LowEnergyDeviceData device_data;
  switch (device_ordinal) {
    case 1:
      device_data.name = kTestDeviceName;
      device_data.address = kTestDeviceAddress1;
      device_data.flags = 0x04;
      device_data.rssi = static_cast<int>(TestRSSI::LOWEST);
      device_data.advertised_uuids = {BluetoothUUID(kTestUUIDGenericAccess),
                                      BluetoothUUID(kTestUUIDGenericAttribute)};
      device_data.service_data = {{BluetoothUUID(kTestUUIDHeartRate), {1}}};
      device_data.manufacturer_data = {{kTestManufacturerId, {1, 2, 3, 4}}};
      device_data.tx_power = static_cast<int>(TestTxPower::LOWEST);
      break;
    case 2:
      device_data.name = kTestDeviceName;
      device_data.address = kTestDeviceAddress1;
      device_data.flags = 0x05;
      device_data.rssi = static_cast<int>(TestRSSI::LOWER);
      device_data.advertised_uuids = {BluetoothUUID(kTestUUIDImmediateAlert),
                                      BluetoothUUID(kTestUUIDLinkLoss)};
      device_data.service_data = {
          {BluetoothUUID(kTestUUIDHeartRate), {}},
          {BluetoothUUID(kTestUUIDImmediateAlert), {0, 2}}};
      device_data.manufacturer_data = {{kTestManufacturerId, {}}};
      device_data.tx_power = static_cast<int>(TestTxPower::LOWER);
      break;
    case 3:
      device_data.name = kTestDeviceNameEmpty;
      device_data.address = kTestDeviceAddress1;
      device_data.rssi = static_cast<int>(TestRSSI::LOW);
      break;
    case 4:
      device_data.name = kTestDeviceNameEmpty;
      device_data.address = kTestDeviceAddress2;
      device_data.rssi = static_cast<int>(TestRSSI::MEDIUM);
      break;
    case 5:
      device_data.address = kTestDeviceAddress1;
      device_data.flags = 0x06;
      device_data.rssi = static_cast<int>(TestRSSI::HIGH);
      break;
    case 6:
      device_data.name = kTestDeviceName;
      device_data.address = kTestDeviceAddress2;
      device_data.flags = 0x18;
      device_data.rssi = static_cast<int>(TestRSSI::LOWEST);
      device_data.transport = BLUETOOTH_TRANSPORT_DUAL;
      break;
    case 7:
      device_data.name = kTestDeviceNameU2f;
      device_data.address = kTestDeviceAddress1;
      device_data.flags = 0x07;
      device_data.rssi = static_cast<int>(TestRSSI::LOWEST);
      device_data.advertised_uuids = {BluetoothUUID(kTestUUIDU2f)};
      device_data.service_data = {
          {BluetoothUUID(kTestUUIDU2fControlPointLength), {0, 20}}};
      break;
    case 8:
      device_data.name = kTestDeviceNameCable;
      device_data.address = kTestDeviceAddress1;
      device_data.flags = 0x07;
      device_data.rssi = static_cast<int>(TestRSSI::LOWEST);
      device_data.service_data = {
          {BluetoothUUID(kTestUUIDCableAdvertisement),
           std::vector<uint8_t>(std::begin(kTestCableEid),
                                std::end(kTestCableEid))}};
      break;
    case 9:
      device_data.name = kTestDeviceNameCable;
      device_data.address = kTestDeviceAddress2;
      device_data.flags = 0x07;
      device_data.rssi = static_cast<int>(TestRSSI::LOWEST);
      device_data.advertised_uuids = {
          BluetoothUUID(kTestUUIDCableAdvertisement),
          BluetoothUUID(kTestUuidFormattedClientEid)};
      break;
    default:
      NOTREACHED();
  }

  return device_data;
}

}  // namespace device
