// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/udev.h"
#include <vector>

#include "base/files/file_path.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "device/udev_linux/udev_loader.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(UdevTest, DecodeString) {
  ASSERT_EQ("", UdevDecodeString(""));
  ASSERT_EQ("\\", UdevDecodeString("\\x5c"));
  ASSERT_EQ("\\x5", UdevDecodeString("\\x5"));
  ASSERT_EQ("049f", UdevDecodeString("049f"));
  ASSERT_EQ(
      "HD Pro Webcam C920", UdevDecodeString("HD\\x20Pro\\x20Webcam\\x20C920"));
  ASSERT_EQ("E-MU Systems,Inc.", UdevDecodeString("E-MU\\x20Systems\\x2cInc."));
}

TEST(UdevTest, Loader) {
  ASSERT_NE(nullptr, UdevLoader::Get());
}

TEST(UdevTest, GetPropertyWithNone) {
  testing::FakeUdevLoader fake_udev;
  udev_device* device =
      fake_udev.AddFakeDevice(/*name=*/"Foo", /*syspath=*/"/device/foo",
                              /*subsystem=*/"", /*devnode=*/std::nullopt,
                              /*devtype=*/std::nullopt, /*sysattrs=*/{},
                              /*properties=*/{});

  const std::string attr_value = UdevDeviceGetPropertyValue(device, "prop");
  EXPECT_TRUE(attr_value.empty());
}

TEST(UdevTest, GetSysPropSimple) {
  testing::FakeUdevLoader fake_udev;
  std::map<std::string, std::string> props;
  props.emplace("prop", "prop value");
  udev_device* device = fake_udev.AddFakeDevice(
      /*name=*/"Foo", /*syspath=*/"/device/foo",
      /*subsystem=*/"", /*devnode=*/std::nullopt, /*devtype=*/std::nullopt,
      /*sysattrs=*/{}, std::move(props));

  std::string attr_value = UdevDeviceGetPropertyValue(device, "prop");
  EXPECT_EQ("prop value", attr_value);

  attr_value = UdevDeviceGetPropertyValue(device, "unknown prop");
  EXPECT_TRUE(attr_value.empty());
}

TEST(UdevTest, GetFullSysPropertiesList) {
  testing::FakeUdevLoader fake_udev;
  std::map<std::string, std::string> props = {
      {"TAGS", ":powerd:"},
      {"DEVNAME", "/dev/dri/card0"},
      {"DEVTYPE", "drm_minor"},
      {"HOTPLUG", "1"},
      {"USEC_INITIALIZED", "1234567"},
      {"MINOR", "0"},
      {"ID_PATH_TAG", "pci-0000_00_02_0"},
      {"PROPERTY", "456"},
      {"DEVPATH", "/devices/pci0000:00/0000:00:02.0/drm/card0"},
      {"ID_PATH", "pci-0000:00:02.0"},
      {"SUBSYSTEM", "drm"},
      {"CONNECTOR", "123"},
      {"MAJOR", "226"},
      {"SEQNUM", "1234"},
      {"ACTION", "change"}};
  udev_device* device = fake_udev.AddFakeDevice(
      /*name=*/"/dev/dri/card0",
      /*syspath=*/"/devices/pci0000:00/0000:00:02.0/drm/card0",
      /*subsystem=*/"drm", /*devnode=*/std::nullopt, /*devtype=*/std::nullopt,
      /*sysattrs=*/{}, std::move(props));

  udev_list_entry* prop_list = udev_device_get_properties_list_entry(device);
  udev_list_entry* entry;
  std::vector<std::string> output_prop_list;
  udev_list_entry_foreach(entry, prop_list) {
    output_prop_list.emplace_back(udev_list_entry_get_name(entry));
  }

  // Expect the properties to be sorted when enumerated.
  EXPECT_THAT(
      output_prop_list,
      testing::UnorderedElementsAre(
          "ACTION", "CONNECTOR", "DEVNAME", "DEVPATH", "DEVTYPE", "HOTPLUG",
          "ID_PATH", "ID_PATH_TAG", "MAJOR", "MINOR", "PROPERTY", "SEQNUM",
          "SUBSYSTEM", "TAGS", "USEC_INITIALIZED"));
}

TEST(UdevTest, GetSysAttrNoAttrs) {
  testing::FakeUdevLoader fake_udev;
  udev_device* device = fake_udev.AddFakeDevice(
      /*name=*/"Foo", /*syspath=*/"/device/foo",
      /*subsystem=*/"", /*devnode=*/std::nullopt, /*devtype=*/std::nullopt,
      /*sysattrs=*/{}, /*properties=*/{});

  const std::string attr_value = UdevDeviceGetSysattrValue(device, "attr");
  EXPECT_TRUE(attr_value.empty());
}

TEST(UdevTest, GetSysAttrSimple) {
  testing::FakeUdevLoader fake_udev;
  std::map<std::string, std::string> attrs;
  attrs.emplace("attr", "attr value");
  udev_device* device = fake_udev.AddFakeDevice(
      /*name=*/"Foo", /*syspath=*/"/device/foo",
      /*subsystem=*/"", /*devnode=*/std::nullopt, /*devtype=*/std::nullopt,
      std::move(attrs), /*properties=*/{});

  std::string attr_value = UdevDeviceGetSysattrValue(device, "attr");
  EXPECT_EQ("attr value", attr_value);

  attr_value = UdevDeviceGetSysattrValue(device, "unknown attr");
  EXPECT_TRUE(attr_value.empty());
}

TEST(UdevTest, GetParent) {
  testing::FakeUdevLoader fake_udev;
  std::map<std::string, std::string> attrs;
  udev_device* grandparent = fake_udev.AddFakeDevice(
      /*name=*/"Foo", /*syspath=*/"/device/foo",
      /*subsystem=*/"", /*devnode=*/std::nullopt, /*devtype=*/std::nullopt,
      /*sysattrs=*/{}, /*properties=*/{});
  udev_device* parent = fake_udev.AddFakeDevice(
      /*name=*/"Foo", /*syspath=*/"/device/foo/bar",
      /*subsystem=*/"", /*devnode=*/std::nullopt, /*devtype=*/std::nullopt,
      /*sysattrs=*/{}, /*properties=*/{});
  udev_device* device = fake_udev.AddFakeDevice(
      /*name=*/"Foo", /*syspath=*/"/device/foo/bar/baz",
      /*subsystem=*/"", /*devnode=*/std::nullopt, /*devtype=*/std::nullopt,
      /*sysattrs=*/{}, /*properties=*/{});

  EXPECT_EQ(parent, udev_device_get_parent(device));
  EXPECT_EQ(grandparent, udev_device_get_parent(parent));
  EXPECT_EQ(nullptr, udev_device_get_parent(grandparent));
}

TEST(UdevTest, GetSysAttrRecursiveOneLevel) {
  testing::FakeUdevLoader fake_udev;
  std::map<std::string, std::string> attrs;
  attrs.emplace("attr", "attr value");
  fake_udev.AddFakeDevice(/*name=*/"Foo", /*syspath=*/"/device/foo",
                          /*subsystem=*/"", /*devnode=*/std::nullopt,
                          /*devtype=*/std::nullopt, std::move(attrs),
                          /*properties=*/{});
  udev_device* device = fake_udev.AddFakeDevice(
      /*name=*/"Foo", /*syspath=*/"/device/foo/bar",
      /*subsystem=*/"", /*devnode=*/std::nullopt, /*devtype=*/std::nullopt,
      /*sysattrs=*/{}, /*properties=*/{});

  // Don't find the attr on the current device.
  std::string attr_value = UdevDeviceGetSysattrValue(device, "attr");
  EXPECT_TRUE(attr_value.empty());

  // Find it when searching recursive.
  attr_value = UdevDeviceRecursiveGetSysattrValue(device, "attr");
  EXPECT_EQ("attr value", attr_value);
}

}  // namespace device
