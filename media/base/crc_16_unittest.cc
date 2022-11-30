// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/crc_16.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(Crc16Test, TestExpectedBUYPASS) {
  static_assert(0x0000 == crc16(""));
  static_assert(0x0186 == crc16("A"));
  static_assert(0xE04F == crc16("chromium"));
  static_assert(0x213B == crc16("One saturday morning at three"
                                "A cheese-monger's shop in Paris"
                                "collapsed to the ground"
                                "with a thunderous sound"
                                "leaving behind only de brie"));
}

}  // namespace media
