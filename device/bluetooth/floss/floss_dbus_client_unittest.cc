// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_dbus_client.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {

class FlossDBusClientTest : public testing::Test {
 public:
  FlossDBusClientTest() = default;
};

// Tests GetDBusTypeInfo generics on container types
TEST_F(FlossDBusClientTest, GetDBusTypeInfoContainers) {
  std::vector<device::BluetoothUUID>* vec_data = nullptr;
  DBusTypeInfo vec_type_info = GetDBusTypeInfo(vec_data);
  EXPECT_EQ("aay", vec_type_info.dbus_signature);
  EXPECT_EQ("vector<BluetoothUUID>", vec_type_info.type_name);

  std::map<uint8_t, std::map<std::string, std::vector<uint16_t>>>* map_data =
      nullptr;
  DBusTypeInfo map_type_info = GetDBusTypeInfo(map_data);
  EXPECT_EQ("a{ya{saq}}", map_type_info.dbus_signature);
  EXPECT_EQ("map<uint8_t, map<string, vector<uint16>>>",
            map_type_info.type_name);

  std::vector<FlossDeviceId>* vec_dict_data = nullptr;
  DBusTypeInfo vec_dict_type_info = GetDBusTypeInfo(vec_dict_data);
  EXPECT_EQ("aa{sv}", vec_dict_type_info.dbus_signature);
  EXPECT_EQ("vector<FlossDeviceId>", vec_dict_type_info.type_name);
}

}  // namespace floss
