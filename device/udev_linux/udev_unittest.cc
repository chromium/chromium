// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/udev.h"
#include "device/udev_linux/udev_loader.h"

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

}  // namespace device
