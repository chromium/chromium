// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bitrate.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(BitrateTest, ConstantBitrate_ToString) {
  Bitrate bitrate = Bitrate::ConstantBitrate(1234u);

  EXPECT_EQ("CBR: 1234 bps", bitrate.ToString());
}

TEST(BitrateTest, VariableBitrate_ToString) {
  Bitrate bitrate = Bitrate::VariableBitrate(0u, 123456u);

  EXPECT_EQ("VBR: target 0 bps, peak 123456 bps", bitrate.ToString());
}

}  // namespace media
