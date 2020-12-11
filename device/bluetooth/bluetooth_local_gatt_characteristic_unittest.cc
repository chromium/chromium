// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/test/bluetooth_gatt_server_test.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothLocalGattCharacteristicTest : public BluetoothGattServerTest {
 public:
  void SetUp() override {
    BluetoothGattServerTest::SetUp();

    StartGattSetup();
    // We will need this device to use with simulating read/write attribute
    // value events.
    device_ = SimulateLowEnergyDevice(1);
    read_characteristic_ = BluetoothLocalGattCharacteristic::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::
            PROPERTY_READ_ENCRYPTED_AUTHENTICATED,
        device::BluetoothLocalGattCharacteristic::Permissions(),
        service_.get());
    write_characteristic_ = BluetoothLocalGattCharacteristic::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::PROPERTY_RELIABLE_WRITE,
        device::BluetoothLocalGattCharacteristic::Permissions(),
        service_.get());
    notify_characteristic_ = BluetoothLocalGattCharacteristic::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::PROPERTY_NOTIFY,
        device::BluetoothLocalGattCharacteristic::Permissions(),
        service_.get());
    indicate_characteristic_ = BluetoothLocalGattCharacteristic::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::PROPERTY_INDICATE,
        device::BluetoothLocalGattCharacteristic::Permissions(),
        service_.get());
    EXPECT_LT(0u, read_characteristic_->GetIdentifier().size());
    EXPECT_LT(0u, write_characteristic_->GetIdentifier().size());
    EXPECT_LT(0u, notify_characteristic_->GetIdentifier().size());
    CompleteGattSetup();
  }

 protected:
  base::WeakPtr<BluetoothLocalGattCharacteristic> read_characteristic_;
  base::WeakPtr<BluetoothLocalGattCharacteristic> write_characteristic_;
  base::WeakPtr<BluetoothLocalGattCharacteristic> notify_characteristic_;
  base::WeakPtr<BluetoothLocalGattCharacteristic> indicate_characteristic_;
  BluetoothDevice* device_;
};

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_ReadLocalCharacteristicValue ReadLocalCharacteristicValue
#else
#define MAYBE_ReadLocalCharacteristicValue DISABLED_ReadLocalCharacteristicValue
#endif
TEST_F(BluetoothLocalGattCharacteristicTest,
       MAYBE_ReadLocalCharacteristicValue) {
  delegate_->value_to_write_ = 0x1337;
  SimulateLocalGattCharacteristicValueReadRequest(
      device_, read_characteristic_.get(), GetReadValueCallback(Call::EXPECTED),
      GetCallback(Call::NOT_EXPECTED));

  EXPECT_EQ(delegate_->value_to_write_, GetInteger(last_read_value_));
  EXPECT_EQ(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_WriteLocalCharacteristicValue WriteLocalCharacteristicValue
#else
#define MAYBE_WriteLocalCharacteristicValue \
  DISABLED_WriteLocalCharacteristicValue
#endif
TEST_F(BluetoothLocalGattCharacteristicTest,
       MAYBE_WriteLocalCharacteristicValue) {
  const uint64_t kValueToWrite = 0x7331ul;
  SimulateLocalGattCharacteristicValueWriteRequest(
      device_, write_characteristic_.get(), GetValue(kValueToWrite),
      GetCallback(Call::EXPECTED), GetCallback(Call::NOT_EXPECTED));

  EXPECT_EQ(kValueToWrite, delegate_->last_written_value_);
  EXPECT_EQ(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_PrepareWriteLocalCharacteristicValue \
  PrepareWriteLocalCharacteristicValue
#else
#define MAYBE_PrepareWriteLocalCharacteristicValue \
  DISABLED_PrepareWriteLocalCharacteristicValue
#endif
TEST_F(BluetoothLocalGattCharacteristicTest,
       MAYBE_PrepareWriteLocalCharacteristicValue) {
  const uint64_t kValueToWrite = 0x7331ul;
  // Clear existing value.
  SimulateLocalGattCharacteristicValueWriteRequest(
      device_, write_characteristic_.get(), GetValue(0),
      GetCallback(Call::EXPECTED), GetCallback(Call::NOT_EXPECTED));

  // Reliable write session is going on.
  SimulateLocalGattCharacteristicValuePrepareWriteRequest(
      device_, write_characteristic_.get(), GetValue(402289342ul), 0, true,
      GetCallback(Call::EXPECTED), GetCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0ul, delegate_->last_written_value_);
  EXPECT_EQ(device_->GetIdentifier(), delegate_->last_seen_device_);

  // Reliable write session ends.
  SimulateLocalGattCharacteristicValuePrepareWriteRequest(
      device_, write_characteristic_.get(), GetValue(kValueToWrite), 0, false,
      GetCallback(Call::EXPECTED), GetCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(kValueToWrite, delegate_->last_written_value_);
  EXPECT_EQ(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_ReadLocalCharacteristicValueFail ReadLocalCharacteristicValueFail
#else
#define MAYBE_ReadLocalCharacteristicValueFail \
  DISABLED_ReadLocalCharacteristicValueFail
#endif
TEST_F(BluetoothLocalGattCharacteristicTest,
       MAYBE_ReadLocalCharacteristicValueFail) {
  delegate_->value_to_write_ = 0x1337;
  delegate_->should_fail_ = true;
  SimulateLocalGattCharacteristicValueReadRequest(
      device_, read_characteristic_.get(),
      GetReadValueCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(delegate_->value_to_write_, GetInteger(last_read_value_));
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_ReadLocalCharacteristicValueWrongPermission \
  ReadLocalCharacteristicValueWrongPermission
#else
#define MAYBE_ReadLocalCharacteristicValueWrongPermission \
  DISABLED_ReadLocalCharacteristicValueWrongPermission
#endif
TEST_F(BluetoothLocalGattCharacteristicTest,
       MAYBE_ReadLocalCharacteristicValueWrongPermission) {
  delegate_->value_to_write_ = 0x1337;
  SimulateLocalGattCharacteristicValueReadRequest(
      device_, write_characteristic_.get(),
      GetReadValueCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(delegate_->value_to_write_, GetInteger(last_read_value_));
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_WriteLocalCharacteristicValueFail \
  WriteLocalCharacteristicValueFail
#else
#define MAYBE_WriteLocalCharacteristicValueFail \
  DISABLED_WriteLocalCharacteristicValueFail
#endif
TEST_F(BluetoothLocalGattCharacteristicTest,
       MAYBE_WriteLocalCharacteristicValueFail) {
  const uint64_t kValueToWrite = 0x7331ul;
  delegate_->should_fail_ = true;
  SimulateLocalGattCharacteristicValueWriteRequest(
      device_, write_characteristic_.get(), GetValue(kValueToWrite),
      GetCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(kValueToWrite, delegate_->last_written_value_);
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_WriteLocalCharacteristicValueWrongPermission \
  WriteLocalCharacteristicValueWrongPermission
#else
#define MAYBE_WriteLocalCharacteristicValueWrongPermission \
  DISABLED_WriteLocalCharacteristicValueWrongPermission
#endif
TEST_F(BluetoothLocalGattCharacteristicTest,
       MAYBE_WriteLocalCharacteristicValueWrongPermission) {
  const uint64_t kValueToWrite = 0x7331ul;
  SimulateLocalGattCharacteristicValueWriteRequest(
      device_, read_characteristic_.get(), GetValue(kValueToWrite),
      GetCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(kValueToWrite, delegate_->last_written_value_);
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_StartAndStopNotifications StartAndStopNotifications
#else
#define MAYBE_StartAndStopNotifications DISABLED_StartAndStopNotifications
#endif
TEST_F(BluetoothLocalGattCharacteristicTest, MAYBE_StartAndStopNotifications) {
  EXPECT_FALSE(SimulateLocalGattCharacteristicNotificationsRequest(
      read_characteristic_.get(), true));
  EXPECT_FALSE(delegate_->NotificationStatusForCharacteristic(
      read_characteristic_.get()));

  EXPECT_FALSE(SimulateLocalGattCharacteristicNotificationsRequest(
      write_characteristic_.get(), true));
  EXPECT_FALSE(delegate_->NotificationStatusForCharacteristic(
      write_characteristic_.get()));

  EXPECT_TRUE(SimulateLocalGattCharacteristicNotificationsRequest(
      notify_characteristic_.get(), true));
  EXPECT_TRUE(delegate_->NotificationStatusForCharacteristic(
      notify_characteristic_.get()));

  EXPECT_TRUE(SimulateLocalGattCharacteristicNotificationsRequest(
      notify_characteristic_.get(), false));
  EXPECT_FALSE(delegate_->NotificationStatusForCharacteristic(
      notify_characteristic_.get()));
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_SendNotifications SendNotifications
#else
#define MAYBE_SendNotifications DISABLED_SendNotifications
#endif
TEST_F(BluetoothLocalGattCharacteristicTest, MAYBE_SendNotifications) {
  const uint64_t kNotifyValue = 0x7331ul;
  EXPECT_EQ(BluetoothLocalGattCharacteristic::NOTIFICATION_SUCCESS,
            notify_characteristic_->NotifyValueChanged(
                nullptr, GetValue(kNotifyValue), false));
  EXPECT_EQ(kNotifyValue, GetInteger(LastNotifactionValueForCharacteristic(
                              notify_characteristic_.get())));

  const uint64_t kIndicateValue = 0x1337ul;
  EXPECT_EQ(BluetoothLocalGattCharacteristic::NOTIFICATION_SUCCESS,
            indicate_characteristic_->NotifyValueChanged(
                nullptr, GetValue(kIndicateValue), true));
  EXPECT_EQ(kIndicateValue, GetInteger(LastNotifactionValueForCharacteristic(
                                indicate_characteristic_.get())));
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_SendNotificationsWrongProperties SendNotificationsWrongProperties
#else
#define MAYBE_SendNotificationsWrongProperties \
  DISABLED_SendNotificationsWrongProperties
#endif
TEST_F(BluetoothLocalGattCharacteristicTest,
       MAYBE_SendNotificationsWrongProperties) {
  const uint64_t kNewValue = 0x3334ul;
  EXPECT_EQ(BluetoothLocalGattCharacteristic::NOTIFY_PROPERTY_NOT_SET,
            read_characteristic_->NotifyValueChanged(
                nullptr, GetValue(kNewValue), false));
  EXPECT_NE(kNewValue, GetInteger(LastNotifactionValueForCharacteristic(
                           read_characteristic_.get())));

  EXPECT_EQ(BluetoothLocalGattCharacteristic::NOTIFY_PROPERTY_NOT_SET,
            write_characteristic_->NotifyValueChanged(
                nullptr, GetValue(kNewValue), false));
  EXPECT_NE(kNewValue, GetInteger(LastNotifactionValueForCharacteristic(
                           write_characteristic_.get())));

  const uint64_t kNotifyValue = 0x7331ul;
  EXPECT_EQ(BluetoothLocalGattCharacteristic::INDICATE_PROPERTY_NOT_SET,
            notify_characteristic_->NotifyValueChanged(
                nullptr, GetValue(kNotifyValue), true));
  EXPECT_NE(kNotifyValue, GetInteger(LastNotifactionValueForCharacteristic(
                              notify_characteristic_.get())));

  const uint64_t kIndicateValue = 0x1337ul;
  EXPECT_EQ(BluetoothLocalGattCharacteristic::NOTIFY_PROPERTY_NOT_SET,
            indicate_characteristic_->NotifyValueChanged(
                nullptr, GetValue(kIndicateValue), false));
  EXPECT_NE(kIndicateValue, GetInteger(LastNotifactionValueForCharacteristic(
                                indicate_characteristic_.get())));
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_SendNotificationsServiceNotRegistered \
  SendNotificationsServiceNotRegistered
#else
#define MAYBE_SendNotificationsServiceNotRegistered \
  DISABLED_SendNotificationsServiceNotRegistered
#endif
TEST_F(BluetoothLocalGattCharacteristicTest,
       MAYBE_SendNotificationsServiceNotRegistered) {
  service_->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));
  const uint64_t kNotifyValue = 0x7331ul;
  EXPECT_EQ(BluetoothLocalGattCharacteristic::SERVICE_NOT_REGISTERED,
            notify_characteristic_->NotifyValueChanged(
                nullptr, GetValue(kNotifyValue), false));
  EXPECT_NE(kNotifyValue, GetInteger(LastNotifactionValueForCharacteristic(
                              notify_characteristic_.get())));
}

}  // namespace device
