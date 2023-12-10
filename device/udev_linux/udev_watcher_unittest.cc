// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/udev_watcher.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr char kSubsystem[] = "subsystem";
constexpr char kDevtype[] = "devtype";
constexpr char kEmptyParam[] = "";

TEST(UdevWatcherTest, FilterParamsReturnCorrectValues) {
  UdevWatcher::Filter subsystem_devtype_filter(kSubsystem, kDevtype);
  EXPECT_TRUE(subsystem_devtype_filter.subsystem());
  EXPECT_TRUE(subsystem_devtype_filter.devtype());
  std::string_view filter_subsystem = subsystem_devtype_filter.subsystem();
  std::string_view filter_devtype = subsystem_devtype_filter.devtype();
  EXPECT_EQ(kSubsystem, filter_subsystem);
  EXPECT_EQ(kDevtype, filter_devtype);
}

TEST(UdevWatcherTest, FilterTreatsEmptyStringAsDontCare) {
  // If an empty string is provided for |subsystem_in| or |devtype_in|,
  // the subsystem or devtype method should return nullptr instead of a
  // zero-length C string. nullptr indicates "don't care" when passed
  // to udev.
  UdevWatcher::Filter subsystem_filter(kSubsystem, kEmptyParam);
  EXPECT_TRUE(subsystem_filter.subsystem());
  EXPECT_FALSE(subsystem_filter.devtype());

  UdevWatcher::Filter devtype_filter(kEmptyParam, kDevtype);
  EXPECT_FALSE(devtype_filter.subsystem());
  EXPECT_TRUE(devtype_filter.devtype());
}

}  // namespace

}  // namespace device
