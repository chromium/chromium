// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_application_service_provider.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "device/bluetooth/dbus/bluetooth_gatt_application_service_provider_impl.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_service_provider_impl.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_service_provider_impl.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_service_provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bluez {

namespace {

const char kAppObjectPath[] = "/fake/hci0/gatt_application";
const char kFakeServiceUuid[] = "00000000-0000-0000-0000-010040008000";
const char kFakeCharacteristicUuid[] = "00000000-0000-0000-0000-010040908000";
const char kFakeDescriptorUuid[] = "00000000-0000-0000-0000-018390008000";

// This is really ugly, but it really is the best way to verify our message
// was constructed correctly. This string was generated from the test data
// and then manually verified to match the expected signature.
const char kExpectedMessage[] =
    "message_type: MESSAGE_METHOD_RETURN\n"
    "signature: a{oa{sa{sv}}}\n"
    "reply_serial: 123\n"
    "\n"
    "array [\n"
    "  dict entry {\n"
    "    object_path \"/fake/hci0/gatt_application/service0\"\n"
    "    array [\n"
    "      dict entry {\n"
    "        string \"org.bluez.GattService1\"\n"
    "        array [\n"
    "          dict entry {\n"
    "            string \"UUID\"\n"
    "            variant               string "
    "\"00000000-0000-0000-0000-010040008000\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Primary\"\n"
    "            variant               bool true\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Includes\"\n"
    "            variant               array [\n"
    "              ]\n"
    "          }\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "  dict entry {\n"
    "    object_path \"/fake/hci0/gatt_application/service1\"\n"
    "    array [\n"
    "      dict entry {\n"
    "        string \"org.bluez.GattService1\"\n"
    "        array [\n"
    "          dict entry {\n"
    "            string \"UUID\"\n"
    "            variant               string "
    "\"00000000-0000-0000-0000-010040008000\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Primary\"\n"
    "            variant               bool true\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Includes\"\n"
    "            variant               array [\n"
    "              ]\n"
    "          }\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "  dict entry {\n"
    "    object_path \"/fake/hci0/gatt_application/service0/characteristic0\"\n"
    "    array [\n"
    "      dict entry {\n"
    "        string \"org.bluez.GattCharacteristic1\"\n"
    "        array [\n"
    "          dict entry {\n"
    "            string \"UUID\"\n"
    "            variant               string "
    "\"00000000-0000-0000-0000-010040908000\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Service\"\n"
    "            variant               object_path "
    "\"/fake/hci0/gatt_application/service0\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Flags\"\n"
    "            variant               array [\n"
    "                string \"read\"\n"
    "                string \"write\"\n"
    "              ]\n"
    "          }\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "  dict entry {\n"
    "    object_path \"/fake/hci0/gatt_application/service0/characteristic1\"\n"
    "    array [\n"
    "      dict entry {\n"
    "        string \"org.bluez.GattCharacteristic1\"\n"
    "        array [\n"
    "          dict entry {\n"
    "            string \"UUID\"\n"
    "            variant               string "
    "\"00000000-0000-0000-0000-010040908000\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Service\"\n"
    "            variant               object_path "
    "\"/fake/hci0/gatt_application/service0\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Flags\"\n"
    "            variant               array [\n"
    "                string \"read\"\n"
    "                string \"write\"\n"
    "              ]\n"
    "          }\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "  dict entry {\n"
    "    object_path \"/fake/hci0/gatt_application/service1/characteristic0\"\n"
    "    array [\n"
    "      dict entry {\n"
    "        string \"org.bluez.GattCharacteristic1\"\n"
    "        array [\n"
    "          dict entry {\n"
    "            string \"UUID\"\n"
    "            variant               string "
    "\"00000000-0000-0000-0000-010040908000\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Service\"\n"
    "            variant               object_path "
    "\"/fake/hci0/gatt_application/service1\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Flags\"\n"
    "            variant               array [\n"
    "                string \"read\"\n"
    "                string \"write\"\n"
    "              ]\n"
    "          }\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "  dict entry {\n"
    "    object_path "
    "\"/fake/hci0/gatt_application/service0/characteristic0/descriptor0\"\n"
    "    array [\n"
    "      dict entry {\n"
    "        string \"org.bluez.GattDescriptor1\"\n"
    "        array [\n"
    "          dict entry {\n"
    "            string \"UUID\"\n"
    "            variant               string "
    "\"00000000-0000-0000-0000-018390008000\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Characteristic\"\n"
    "            variant               object_path "
    "\"/fake/hci0/gatt_application/service0/characteristic0\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Flags\"\n"
    "            variant               array [\n"
    "                string \"read\"\n"
    "                string \"write\"\n"
    "              ]\n"
    "          }\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "  dict entry {\n"
    "    object_path "
    "\"/fake/hci0/gatt_application/service0/characteristic1/descriptor1\"\n"
    "    array [\n"
    "      dict entry {\n"
    "        string \"org.bluez.GattDescriptor1\"\n"
    "        array [\n"
    "          dict entry {\n"
    "            string \"UUID\"\n"
    "            variant               string "
    "\"00000000-0000-0000-0000-018390008000\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Characteristic\"\n"
    "            variant               object_path "
    "\"/fake/hci0/gatt_application/service0/characteristic1\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Flags\"\n"
    "            variant               array [\n"
    "                string \"read\"\n"
    "                string \"write\"\n"
    "              ]\n"
    "          }\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "  dict entry {\n"
    "    object_path "
    "\"/fake/hci0/gatt_application/service1/characteristic0/descriptor2\"\n"
    "    array [\n"
    "      dict entry {\n"
    "        string \"org.bluez.GattDescriptor1\"\n"
    "        array [\n"
    "          dict entry {\n"
    "            string \"UUID\"\n"
    "            variant               string "
    "\"00000000-0000-0000-0000-018390008000\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Characteristic\"\n"
    "            variant               object_path "
    "\"/fake/hci0/gatt_application/service1/characteristic0\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Flags\"\n"
    "            variant               array [\n"
    "                string \"read\"\n"
    "                string \"write\"\n"
    "              ]\n"
    "          }\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "  dict entry {\n"
    "    object_path "
    "\"/fake/hci0/gatt_application/service0/characteristic0/descriptor3\"\n"
    "    array [\n"
    "      dict entry {\n"
    "        string \"org.bluez.GattDescriptor1\"\n"
    "        array [\n"
    "          dict entry {\n"
    "            string \"UUID\"\n"
    "            variant               string "
    "\"00000000-0000-0000-0000-018390008000\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Characteristic\"\n"
    "            variant               object_path "
    "\"/fake/hci0/gatt_application/service0/characteristic0\"\n"
    "          }\n"
    "          dict entry {\n"
    "            string \"Flags\"\n"
    "            variant               array [\n"
    "                string \"read\"\n"
    "                string \"write\"\n"
    "              ]\n"
    "          }\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "]\n";

void ResponseSenderCallback(const std::string& expected_message,
                            std::unique_ptr<dbus::Response> response) {
  EXPECT_EQ(expected_message, response->ToString());
}

}  // namespace

class BluetoothGattApplicationServiceProviderTest : public testing::Test {
 public:
  std::string CreateFakeService(
      BluetoothGattApplicationServiceProviderImpl* app_provider,
      const std::string& service_path) {
    const std::string& full_service_path =
        std::string(kAppObjectPath) + "/" + service_path;
    app_provider->service_providers_.push_back(
        std::make_unique<BluetoothGattServiceServiceProviderImpl>(
            nullptr, dbus::ObjectPath(full_service_path), kFakeServiceUuid,
            true, std::vector<dbus::ObjectPath>()));
    return full_service_path;
  }

  std::string CreateFakeCharacteristic(
      BluetoothGattApplicationServiceProviderImpl* app_provider,
      const std::string& characteristic_path,
      const std::string& service_path) {
    const std::string& full_characteristic_path =
        service_path + "/" + characteristic_path;
    app_provider->characteristic_providers_.push_back(
        base::WrapUnique(new BluetoothGattCharacteristicServiceProviderImpl(
            nullptr, dbus::ObjectPath(full_characteristic_path), nullptr,
            kFakeCharacteristicUuid,
            std::vector<std::string>({"read", "write"}),
            dbus::ObjectPath(service_path))));
    return full_characteristic_path;
  }

  void CreateFakeDescriptor(
      BluetoothGattApplicationServiceProviderImpl* app_provider,
      const std::string& descriptor_path,
      const std::string& characteristic_path) {
    const std::string& full_descriptor_path =
        characteristic_path + "/" + descriptor_path;
    app_provider->descriptor_providers_.push_back(
        base::WrapUnique(new BluetoothGattDescriptorServiceProviderImpl(
            nullptr, dbus::ObjectPath(full_descriptor_path), nullptr,
            kFakeDescriptorUuid, std::vector<std::string>({"read", "write"}),
            dbus::ObjectPath(characteristic_path))));
  }

  void CreateFakeAttributes(
      BluetoothGattApplicationServiceProviderImpl* app_provider) {
    const std::string& kServicePath1 =
        CreateFakeService(app_provider, "service0");
    const std::string& kServicePath2 =
        CreateFakeService(app_provider, "service1");

    const std::string& kCharacteristicPath1 = CreateFakeCharacteristic(
        app_provider, "characteristic0", kServicePath1);
    const std::string& kCharacteristicPath2 = CreateFakeCharacteristic(
        app_provider, "characteristic1", kServicePath1);
    const std::string& kCharacteristicPath3 = CreateFakeCharacteristic(
        app_provider, "characteristic0", kServicePath2);

    CreateFakeDescriptor(app_provider, "descriptor0", kCharacteristicPath1);
    CreateFakeDescriptor(app_provider, "descriptor1", kCharacteristicPath2);
    CreateFakeDescriptor(app_provider, "descriptor2", kCharacteristicPath3);
    CreateFakeDescriptor(app_provider, "descriptor3", kCharacteristicPath1);
  }
};

TEST_F(BluetoothGattApplicationServiceProviderTest, GetManagedObjects) {
  std::unique_ptr<BluetoothGattApplicationServiceProviderImpl> app_provider =
      std::make_unique<BluetoothGattApplicationServiceProviderImpl>(
          nullptr, dbus::ObjectPath(kAppObjectPath),
          std::map<dbus::ObjectPath,
                   raw_ptr<BluetoothLocalGattServiceBlueZ, CtnExperimental>>());
  CreateFakeAttributes(app_provider.get());

  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  app_provider->GetManagedObjects(
      &method_call, base::BindOnce(&ResponseSenderCallback, kExpectedMessage));
}

TEST_F(BluetoothGattApplicationServiceProviderTest, SendValueChanged) {
  std::unique_ptr<BluetoothGattApplicationServiceProviderImpl> app_provider =
      std::make_unique<BluetoothGattApplicationServiceProviderImpl>(
          nullptr, dbus::ObjectPath(kAppObjectPath),
          std::map<dbus::ObjectPath,
                   raw_ptr<BluetoothLocalGattServiceBlueZ, CtnExperimental>>());
  const std::string& kServicePath =
      CreateFakeService(app_provider.get(), "service0");
  const std::string& kCharacteristicPath = CreateFakeCharacteristic(
      app_provider.get(), "characteristic0", kServicePath);

  std::vector<uint8_t> kNewValue = {0x13, 0x37, 0xba, 0xad, 0xf0};
  app_provider->SendValueChanged(dbus::ObjectPath(kCharacteristicPath),
                                 kNewValue);
  // TODO(rkc): Write a test implementation of dbus::Bus and
  // dbus::ExportedObject so we can capture the actual signal that is sent and
  // verify its contents.
}

}  // namespace bluez
