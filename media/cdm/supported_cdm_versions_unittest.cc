// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/supported_cdm_versions.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(SupportedCdmVersions, IsSupportedCdmInterfaceVersion) {
  EXPECT_FALSE(IsSupportedCdmInterfaceVersion(9));
  EXPECT_TRUE(IsSupportedCdmInterfaceVersion(10));
  EXPECT_TRUE(IsSupportedCdmInterfaceVersion(11));
  EXPECT_TRUE(IsSupportedCdmInterfaceVersion(12));
  EXPECT_FALSE(IsSupportedCdmInterfaceVersion(13));
}

TEST(SupportedCdmVersions, IsSupportedCdmHostVersion) {
  EXPECT_FALSE(IsSupportedCdmHostVersion(9));
  EXPECT_TRUE(IsSupportedCdmHostVersion(10));
  EXPECT_TRUE(IsSupportedCdmHostVersion(11));
  EXPECT_TRUE(IsSupportedCdmHostVersion(12));
  EXPECT_FALSE(IsSupportedCdmHostVersion(13));
}

TEST(SupportedCdmVersions, IsSupportedAndEnabledCdmInterfaceVersion) {
  EXPECT_FALSE(IsSupportedAndEnabledCdmInterfaceVersion(9));
  EXPECT_TRUE(IsSupportedAndEnabledCdmInterfaceVersion(10));
  EXPECT_TRUE(IsSupportedAndEnabledCdmInterfaceVersion(11));
  EXPECT_FALSE(IsSupportedAndEnabledCdmInterfaceVersion(12));
  EXPECT_FALSE(IsSupportedAndEnabledCdmInterfaceVersion(13));
}

}  // namespace media
