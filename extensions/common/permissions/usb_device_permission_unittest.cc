// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "components/version_info/channel.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "extensions/common/permissions/usb_device_permission_data.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// ID of test USB device config to which interfaces described by
// |interface_classes| parameter of |CreateTestUsbDevice| should be added.
const int kUsbConfigWithInterfaces = 1;

// ID of test USB device config created by |CreateTestUsbDevice| that contains
// no interfaces.
const int kUsbConfigWithoutInterfaces = 2;

scoped_refptr<device::FakeUsbDeviceInfo> CreateTestUsbDevice(
    uint16_t vendor_id,
    uint16_t product_id,
    uint8_t device_class,
    const std::vector<uint8_t> interface_classes) {
  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  auto config_1 = device::mojom::UsbConfigurationInfo::New();
  config_1->configuration_value = kUsbConfigWithInterfaces;
  configs.push_back(std::move(config_1));

  auto config_2 = device::mojom::UsbConfigurationInfo::New();
  config_2->configuration_value = kUsbConfigWithoutInterfaces;
  configs.push_back(std::move(config_2));

  for (size_t i = 0; i < interface_classes.size(); ++i) {
    auto alternate = device::mojom::UsbAlternateInterfaceInfo::New();
    alternate->alternate_setting = 0;
    alternate->class_code = interface_classes[i];
    alternate->subclass_code = 255;
    alternate->protocol_code = 255;

    auto interface = device::mojom::UsbInterfaceInfo::New();
    interface->interface_number = i;
    interface->alternates.push_back(std::move(std::move(alternate)));

    configs[0]->interfaces.push_back(std::move(interface));
  }

  return new device::FakeUsbDeviceInfo(vendor_id, product_id, device_class,
                                       std::move(configs));
}

scoped_refptr<const Extension> CreateTestApp(
    base::Value usb_device_permission) {
  return ExtensionBuilder()
      .SetManifest(
          base::Value::Dict()
              .Set("name", "test app")
              .Set("version", "1")
              .Set("app", base::Value::Dict().Set(
                              "background",
                              base::Value::Dict().Set(
                                  "scripts",
                                  base::Value::List().Append("background.js"))))
              .Set("permissions",
                   base::Value::List().Append("usb").Append(
                       base::Value::Dict().Set(
                           "usbDevices", base::Value::List().Append(std::move(
                                             usb_device_permission))))))
      .Build();
}

}  // namespace

TEST(USBDevicePermissionTest, PermissionDataOrder) {
  EXPECT_LT(UsbDevicePermissionData(0x02ad, 0x138c, -1, -1),
            UsbDevicePermissionData(0x02ad, 0x138d, -1, -1));
  ASSERT_LT(UsbDevicePermissionData(0x02ad, 0x138d, -1, -1),
            UsbDevicePermissionData(0x02ae, 0x138c, -1, -1));
  EXPECT_LT(UsbDevicePermissionData(0x02ad, 0x138c, 1, 3),
            UsbDevicePermissionData(0x02ad, 0x138c, 2, 2));
  EXPECT_LT(UsbDevicePermissionData(0x02ad, 0x138c, 1, 2),
            UsbDevicePermissionData(0x02ad, 0x138c, 1, 3));
}

TEST(USBDevicePermissionTest, CheckVendorAndProductId) {
  base::Value permission_data_value(
      base::Value::Dict().Set("vendorId", 0x02ad).Set("productId", 0x138c));

  UsbDevicePermissionData permission_data;
  ASSERT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x02ad, 0x138c,
            UsbDevicePermissionData::SPECIAL_VALUE_UNSPECIFIED);

    EXPECT_TRUE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x138c, 0x02ad,
            UsbDevicePermissionData::SPECIAL_VALUE_UNSPECIFIED);

    EXPECT_FALSE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x02ad, 0x138c, 3);

    EXPECT_FALSE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, CheckInterfaceId) {
  base::Value permission_data_value(base::Value::Dict()
                                        .Set("vendorId", 0x02ad)
                                        .Set("productId", 0x138c)
                                        .Set("interfaceId", 3));
  UsbDevicePermissionData permission_data;
  ASSERT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));
  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x02ad, 0x138c, 3);

    EXPECT_TRUE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x02ad, 0x138c, 2);

    EXPECT_FALSE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x02ad, 0x138c,
            UsbDevicePermissionData::SPECIAL_VALUE_UNSPECIFIED);

    EXPECT_TRUE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, InterfaceClass) {
  base::Value permission_data_value(
      base::Value::Dict().Set("interfaceClass", 3));
  UsbDevicePermissionData permission_data;
  EXPECT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x02ad, 0x138c,
            UsbDevicePermissionData::SPECIAL_VALUE_UNSPECIFIED);

    // Interface class permission should be ignored when not in kiosk session.
    EXPECT_FALSE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x02ad, 0x138c,
            UsbDevicePermissionData::SPECIAL_VALUE_UNSPECIFIED);

    // |ForDeviceWithAnyInterfaceClass| creates check param that matches any
    // interfaceClass permission property - needed in cases where set of
    // interface classes supported by a USB device is not known.
    // In this case, there is a usbDevice permisison with only interfaceClass
    // set, so the param is accepted.
    EXPECT_TRUE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, InterfaceClassWithVendorId) {
  base::Value permission_data_value(
      base::Value::Dict().Set("vendorId", 0x02ad).Set("interfaceClass", 3));
  UsbDevicePermissionData permission_data;
  EXPECT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x02ad, 0x138c,
            UsbDevicePermissionData::SPECIAL_VALUE_UNSPECIFIED);

    EXPECT_FALSE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x02ad, 0x138c,
            UsbDevicePermissionData::SPECIAL_VALUE_UNSPECIFIED);

    EXPECT_TRUE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
            app.get(), 0x138c, 0x138c,
            UsbDevicePermissionData::SPECIAL_VALUE_UNSPECIFIED);

    EXPECT_FALSE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, CheckHidUsbAgainstInterfaceClass) {
  base::Value permission_data_value(
      base::Value::Dict().Set("vendorId", 0x02ad).Set("interfaceClass", 3));
  UsbDevicePermissionData permission_data;
  EXPECT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForHidDevice(app.get(), 0x02ad,
                                                      0x138c);

    EXPECT_FALSE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForHidDevice(app.get(), 0x02ad,
                                                      0x138c);

    EXPECT_TRUE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForHidDevice(app.get(), 0x138c,
                                                      0x138c);

    EXPECT_FALSE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, CheckHidUsbAgainstDeviceIds) {
  base::Value permission_data_value(
      base::Value::Dict().Set("vendorId", 0x02ad).Set("productId", 0x138c));
  UsbDevicePermissionData permission_data;
  EXPECT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForHidDevice(app.get(), 0x02ad,
                                                      0x138c);

    EXPECT_TRUE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForHidDevice(app.get(), 0x138c,
                                                      0x138c);

    EXPECT_FALSE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, CheckDeviceAgainstDeviceIds) {
  base::Value permission_data_value(
      base::Value::Dict().Set("vendorId", 0x02ad).Set("productId", 0x138c));
  UsbDevicePermissionData permission_data;
  EXPECT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0x9, std::vector<uint8_t>());
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_TRUE(permission_data.Check(param.get()));
  }

  {
    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x138c, 0x138c, 0x9, std::vector<uint8_t>());
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_FALSE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, CheckDeviceAgainstDeviceClass) {
  base::Value permission_data_value(
      base::Value::Dict().Set("interfaceClass", 0x9));
  UsbDevicePermissionData permission_data;
  EXPECT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0x9, std::vector<uint8_t>());
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_FALSE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0x9, std::vector<uint8_t>());
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_TRUE(permission_data.Check(param.get()));
  }
  {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0x3, std::vector<uint8_t>());
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_FALSE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, IgnoreNullDeviceClass) {
  base::Value permission_data_value(
      base::Value::Dict().Set("interfaceClass", 0));
  UsbDevicePermissionData permission_data;
  EXPECT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0, std::vector<uint8_t>());
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_FALSE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, CheckDeviceAgainstInterfaceClass) {
  base::Value permission_data_value(
      base::Value::Dict().Set("interfaceClass", 0x3));
  UsbDevicePermissionData permission_data;
  EXPECT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0, {2, 3});
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_FALSE(permission_data.Check(param.get()));
  }

  {
    // Interface should match inactive configuration when none of configurations
    // is active.
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0, {2, 3});
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_TRUE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0, {2, 3});
    device->SetActiveConfig(kUsbConfigWithInterfaces);
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_TRUE(permission_data.Check(param.get()));
  }

  {
    // Interface should match inactive configuration when another configuration
    // is active.
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0, {2, 3});
    device->SetActiveConfig(kUsbConfigWithoutInterfaces);
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_TRUE(permission_data.Check(param.get()));
  }

  {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
        scoped_session_type(
            ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk));
    ScopedCurrentChannel channel(version_info::Channel::DEV);

    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0, {4, 5});
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(app.get(),
                                                      device->GetDeviceInfo());

    EXPECT_FALSE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, CheckDeviceAndInterfaceId) {
  base::Value permission_data_value(base::Value::Dict()
                                        .Set("vendorId", 0x02ad)
                                        .Set("productId", 0x138c)
                                        .Set("interfaceId", 3));
  UsbDevicePermissionData permission_data;
  EXPECT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0, std::vector<uint8_t>());
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDeviceAndInterface(
            app.get(), device->GetDeviceInfo(), 3);

    EXPECT_TRUE(permission_data.Check(param.get()));
  }

  {
    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0, std::vector<uint8_t>());
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDeviceAndInterface(
            app.get(), device->GetDeviceInfo(), 2);

    EXPECT_FALSE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest,
     CheckDeviceAndInterfaceIDAgainstMissingInterfaceId) {
  base::Value permission_data_value(
      base::Value::Dict().Set("vendorId", 0x02ad).Set("productId", 0x138c));
  UsbDevicePermissionData permission_data;
  EXPECT_TRUE(permission_data.FromValue(&permission_data_value));

  scoped_refptr<const Extension> app =
      CreateTestApp(std::move(permission_data_value));

  {
    scoped_refptr<device::FakeUsbDeviceInfo> device =
        CreateTestUsbDevice(0x02ad, 0x138c, 0, std::vector<uint8_t>());
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDeviceAndInterface(
            app.get(), device->GetDeviceInfo(), 3);

    EXPECT_FALSE(permission_data.Check(param.get()));
  }
}

TEST(USBDevicePermissionTest, InvalidPermission_NoVendorId) {
  base::Value permission_data_value(
      base::Value::Dict().Set("productId", 0x138c).Set("interfaceClass", 3));
  UsbDevicePermissionData permission_data;
  ASSERT_FALSE(permission_data.FromValue(&permission_data_value));
}

TEST(USBDevicePermissionTest, InvalidPermission_OnlyVendorId) {
  base::Value permission_data_value(
      base::Value::Dict().Set("vendorId", 0x02ad));
  UsbDevicePermissionData permission_data;
  ASSERT_FALSE(permission_data.FromValue(&permission_data_value));
}

TEST(USBDevicePermissionTest, InvalidPermission_NoProductIdWithInterfaceId) {
  base::Value permission_data_value(
      base::Value::Dict().Set("vendorId", 0x02ad).Set("interfaceId", 3));
  UsbDevicePermissionData permission_data;
  ASSERT_FALSE(permission_data.FromValue(&permission_data_value));
}

TEST(USBDevicePermissionTest, RejectInterfaceIdIfInterfaceClassPresent) {
  base::Value permission_data_value(base::Value::Dict()
                                        .Set("vendorId", 0x02ad)
                                        .Set("productId", 0x128c)
                                        .Set("interfaceId", 3)
                                        .Set("interfaceClass", 7));
  UsbDevicePermissionData permission_data;
  ASSERT_FALSE(permission_data.FromValue(&permission_data_value));
}

}  // namespace extensions
