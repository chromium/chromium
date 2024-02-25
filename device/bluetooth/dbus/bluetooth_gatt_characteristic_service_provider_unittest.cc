// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/dbus/bluetooth_gatt_attribute_value_delegate.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_delegate_wrapper.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_service_provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bluez {

TEST(BluetoothGattCharacteristicServiceProviderTest, ReadValueSuccess) {
  auto method_call =
      std::make_unique<dbus::MethodCall>("com.example.Interface", "SomeMethod");
  method_call->SetSerial(123);
  method_call->SetReplySerial(456);
  bool callback_called = false;

  BluetoothGattCharacteristicServiceProviderImpl provider_impl(
      /*bus=*/nullptr,
      /*object_path=*/dbus::ObjectPath(),
      /*delegate=*/std::unique_ptr<BluetoothGattAttributeValueDelegate>(),
      /*uuid=*/std::string(),
      /*flags=*/std::vector<std::string>(),
      /*service_path=*/dbus::ObjectPath());

  const std::vector<uint8_t> read_value = {1, 2, 3};
  provider_impl.OnReadValue(
      method_call.get(),
      base::BindLambdaForTesting([&callback_called, read_value](
                                     std::unique_ptr<dbus::Response> response) {
        EXPECT_EQ(response->GetMessageType(), DBUS_MESSAGE_TYPE_METHOD_RETURN);
        dbus::MessageReader reader(response.get());
        EXPECT_EQ(reader.GetDataType(), dbus::Message::ARRAY);
        const uint8_t* bytes = nullptr;
        size_t length = 0;
        EXPECT_TRUE(reader.PopArrayOfBytes(&bytes, &length));
        EXPECT_EQ(length, read_value.size());
        callback_called = true;
      }),
      /*error_code=*/std::nullopt, read_value);

  EXPECT_TRUE(callback_called);
}

TEST(BluetoothGattCharacteristicServiceProviderTest, ReadValueFailure) {
  auto method_call =
      std::make_unique<dbus::MethodCall>("com.example.Interface", "SomeMethod");
  method_call->SetSerial(123);
  method_call->SetReplySerial(456);
  bool callback_called = false;

  BluetoothGattCharacteristicServiceProviderImpl provider_impl(
      /*bus=*/nullptr,
      /*object_path=*/dbus::ObjectPath(),
      /*delegate=*/std::unique_ptr<BluetoothGattAttributeValueDelegate>(),
      /*uuid=*/std::string(),
      /*flags=*/std::vector<std::string>(),
      /*service_path=*/dbus::ObjectPath());

  const std::vector<uint8_t> read_value = {1, 2, 3};
  provider_impl.OnReadValue(
      method_call.get(),
      base::BindLambdaForTesting(
          [&callback_called](std::unique_ptr<dbus::Response> response) {
            EXPECT_EQ(response->GetMessageType(), DBUS_MESSAGE_TYPE_ERROR);
            dbus::MessageReader reader(response.get());
            EXPECT_NE(reader.GetDataType(), dbus::Message::ARRAY);
            const uint8_t* bytes = nullptr;
            size_t length = 0;
            EXPECT_FALSE(reader.PopArrayOfBytes(&bytes, &length));
            callback_called = true;
          }),
      device::BluetoothGattService::GattErrorCode::kFailed, read_value);

  EXPECT_TRUE(callback_called);
}

}  // namespace bluez
