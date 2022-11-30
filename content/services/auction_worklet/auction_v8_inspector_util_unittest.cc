// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_inspector_util.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-inspector.h"

using testing::ElementsAre;

namespace auction_worklet {

TEST(AuctionV8InspectorUtilTest, GetStringBytes8) {
  const uint8_t chars[] = {'a', 'b', 'c', 0xD0, 0xB0, 0xD0, 0xB1, 0xD0, 0xB2};
  auto string_buf = v8_inspector::StringBuffer::create(
      v8_inspector::StringView(chars, std::size(chars)));
  EXPECT_THAT(GetStringBytes(string_buf.get()),
              ElementsAre('a', 'b', 'c', 0xD0, 0xB0, 0xD0, 0xB1, 0xD0, 0xB2));
}

TEST(AuctionV8InspectorUtilTest, GetStringBytes16) {
  const uint16_t chars16[] = {0x414,  // CYRILLIC CAPITAL LETTER DE
                              0x44F,  // CYRILLIC SMALL LETTER YA
                              0x43A,  // CYRILLIC SMALL LETTER KA
                              0x443,  // CYRILLIC SMALL LETTER U
                              0x44E,  // CYRILLIC SMALL LETTER YU
                              '!',
                              // U+1F600, GRINNING FACE, as surrogate pairs.
                              0xD83D, 0xDE00};
  auto string_buf = v8_inspector::StringBuffer::create(
      v8_inspector::StringView(chars16, std::size(chars16)));
  EXPECT_THAT(GetStringBytes(string_buf.get()),
              ElementsAre(0xD0, 0x94, 0xD1, 0x8F, 0xD0, 0xBA, 0xD1, 0x83, 0xD1,
                          0x8E, '!', 0xF0, 0x9F, 0x98, 0x80));
}

}  // namespace auction_worklet
