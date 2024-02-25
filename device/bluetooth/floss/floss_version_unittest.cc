// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_version.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {

class FlossVersionTest : public testing::Test {
 public:
  FlossVersionTest() = default;
};

TEST_F(FlossVersionTest, IntoVersionTest) {
  base::Version version = floss::version::IntoVersion(0x00640012);
  EXPECT_EQ(version, base::Version("100.18"));
}

TEST_F(FlossVersionTest, SupportVersionRangeTest) {
  base::Version minVersion = base::Version("0.0");
  base::Version maxVersion = base::Version("65535.65535");
  base::Version minSupportedVersion =
      floss::version::GetMinimalSupportedVersion();
  base::Version maxSupportedVersion =
      floss::version::GetMaximalSupportedVersion();

  EXPECT_TRUE(minVersion <= minSupportedVersion);
  EXPECT_TRUE(maxVersion >= maxSupportedVersion);
  EXPECT_TRUE(minSupportedVersion <= maxSupportedVersion);
}

}  // namespace floss
