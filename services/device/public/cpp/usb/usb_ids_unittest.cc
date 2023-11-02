// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "services/device/public/cpp/usb/usb_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const uint16_t kGoogleVendorId = 0x18d1;
static const uint16_t kNexusSProductId = 0x4e21;

}  // namespace

namespace device {

TEST(UsbIdsTest, GetVendorName) {
  EXPECT_EQ(NULL, UsbIds::GetVendorName(0));
  EXPECT_EQ(std::string("Google Inc."), UsbIds::GetVendorName(kGoogleVendorId));
}

TEST(UsbIdsTest, GetProductName) {
  EXPECT_EQ(NULL, UsbIds::GetProductName(0, 0));
  EXPECT_EQ(NULL, UsbIds::GetProductName(kGoogleVendorId, 0));
  EXPECT_EQ(std::string("Nexus S"),
            UsbIds::GetProductName(kGoogleVendorId, kNexusSProductId));
}

}  // namespace device
