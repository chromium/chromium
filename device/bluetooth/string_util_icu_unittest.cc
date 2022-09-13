// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/string_util_icu.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(StringUtilICUTest, HasGraphicCharacter) {
  // Contain Graphic Characters.
  EXPECT_TRUE(HasGraphicCharacter("A"));
  EXPECT_TRUE(HasGraphicCharacter("#"));
  EXPECT_TRUE(HasGraphicCharacter("3"));
  EXPECT_TRUE(HasGraphicCharacter("\u2764" /* Heart Symbol */));
  EXPECT_TRUE(HasGraphicCharacter("   A"));
  EXPECT_TRUE(HasGraphicCharacter("A   "));
  EXPECT_TRUE(HasGraphicCharacter("   A   "));
  std::string nulls_at_start(5, '\0');
  nulls_at_start[4] = 'A';
  EXPECT_TRUE(HasGraphicCharacter(nulls_at_start));

  std::string nulls_at_end(5, '\0');
  nulls_at_end[0] = 'A';
  EXPECT_TRUE(HasGraphicCharacter(nulls_at_end));

  std::string surrounded_by_nulls(5, '\0');
  surrounded_by_nulls[2] = 'A';
  EXPECT_TRUE(HasGraphicCharacter(surrounded_by_nulls));

  // Do not contain Graphic Characters.
  EXPECT_FALSE(HasGraphicCharacter(""));
  EXPECT_FALSE(HasGraphicCharacter(std::string(3, '\0')));
  EXPECT_FALSE(HasGraphicCharacter("\n\t\v\b\r "));
  // Unicode Control characters.
  EXPECT_FALSE(HasGraphicCharacter("\u0000\u001f\u007f\u009f"));
}

}  // namespace device
