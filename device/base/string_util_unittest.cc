// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/public/cpp/string_util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(StringUtilTest, HasGraphicCharacter) {
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
  // Directional isolate characters alone do not count as graphic characters.
  EXPECT_FALSE(HasGraphicCharacter("\u2066\u2067\u2068\u2069"));
  // Unicode Control characters.
  EXPECT_FALSE(HasGraphicCharacter("\u0000\u001f\u007f\u009f"));
  // Non-UTF-8 byte sequences are handled safely without crashing.
  EXPECT_FALSE(HasGraphicCharacter("\xff\xfe\xfd"));
  EXPECT_FALSE(HasGraphicCharacter("Valid prefix \xff\xfe"));
}

TEST(StringUtilTest, ContainStringEmpty) {
  EXPECT_EQ(u"", ContainStringForDisplay(u""));
  EXPECT_EQ(u"", ContainStringForDisplay(u"   "));
  EXPECT_EQ(u"", ContainStringForDisplay(u" \r\n\t "));
  EXPECT_EQ(u"", ContainStringForDisplay(u"\u2066"));
  EXPECT_EQ(u"", ContainStringForDisplay(u"\u2067"));
  EXPECT_EQ(u"", ContainStringForDisplay(u"\u2068"));
  EXPECT_EQ(u"", ContainStringForDisplay(u"\u2068\u2069"));
  EXPECT_EQ(u"", ContainStringForDisplay(u"  \u2068\u2069  "));
  EXPECT_EQ(u"", ContainStringForDisplay(u"\u2069\u2069"));
  EXPECT_EQ(u"", ContainStringForDisplay(u"  \u2069  "));
  EXPECT_EQ(u"", ContainStringForDisplay(u"\u2069  \u2069"));
}

TEST(StringUtilTest, ContainStringLeavesBenignStringsLegible) {
  EXPECT_EQ(u"\u2068Logitech MX Keys\u2069",
            ContainStringForDisplay(u"Logitech MX Keys"));
}

TEST(StringUtilTest, ContainStringPreservesSpoofedString) {
  // The override is retained, and neutralized by the enclosing isolate.
  EXPECT_EQ(u"\u2068\u202eLogitech MX Keys\u2069",
            ContainStringForDisplay(u"\u202eLogitech MX Keys"));
}

TEST(StringUtilTest, ContainStringCollapsesWhitespace) {
  EXPECT_EQ(u"\u2068Malicious Device Verified by ChromeOS\u2069",
            ContainStringForDisplay(u"Malicious Device\nVerified by ChromeOS"));
  EXPECT_EQ(u"\u2068Speaker Connection secure\u2069",
            ContainStringForDisplay(u"Speaker        Connection secure"));
}

TEST(StringUtilTest, ContainStringDropsUnmatchedPdi) {
  EXPECT_EQ(u"\u2068Speaker\u2069", ContainStringForDisplay(u"\u2069Speaker"));
  EXPECT_EQ(u"\u2068Speaker\u2069",
            ContainStringForDisplay(u"\u2069  Speaker"));
  EXPECT_EQ(u"\u2068Speaker\u2069",
            ContainStringForDisplay(u"Speaker  \u2069"));
  EXPECT_EQ(u"\u2068Foo Bar\u2069", ContainStringForDisplay(u"Foo \u2069 Bar"));
  EXPECT_EQ(
      u"\u2068Foo Bar\u2069",
      ContainStringForDisplay(ContainStringForDisplay(u"Foo \u2069 Bar")));
  EXPECT_EQ(u"\u2068\u2067Speaker\u2069\u2069",
            ContainStringForDisplay(u"\u2067Speaker\u2069"));
}

TEST(StringUtilTest, ContainStringBalancesUnclosedIsolates) {
  // Unclosed isolate initiators (LRI, RLI, FSI) are balanced with PDIs before
  // the outer terminating PDI so directional formatting cannot leak out.
  EXPECT_EQ(u"\u2068\u2066Speaker\u2069\u2069",
            ContainStringForDisplay(u"\u2066Speaker"));
  EXPECT_EQ(u"\u2068\u2067Speaker\u2069\u2069",
            ContainStringForDisplay(u"\u2067Speaker"));
  EXPECT_EQ(u"\u2068\u2068Speaker\u2069\u2069",
            ContainStringForDisplay(u"\u2068Speaker"));
  EXPECT_EQ(u"\u2068\u2066\u2067Speaker\u2069\u2069\u2069",
            ContainStringForDisplay(u"\u2066\u2067Speaker"));

  // If truncation cuts off a PDI, the unclosed isolate is still balanced.
  // Directional isolates are excluded from the truncation length budget, so 63
  // 'A's + ellipsis are retained.
  const std::u16string truncated_pair =
      u"\u2067" + std::u16string(70, u'A') + u"\u2069";
  EXPECT_EQ(u"\u2068\u2067" + std::u16string(63, u'A') + u"\u2069\u2026\u2069",
            ContainStringForDisplay(truncated_pair));
}

TEST(StringUtilTest, ContainStringTruncates) {
  const std::u16string name_64(64, u'A');
  EXPECT_EQ(u"\u2068" + name_64 + u"\u2069", ContainStringForDisplay(name_64));

  const std::u16string name_65(65, u'A');
  EXPECT_EQ(u"\u2068" + std::u16string(63, u'A') + u"\u2026\u2069",
            ContainStringForDisplay(name_65));

  const std::u16string name_200(200, u'A');
  EXPECT_EQ(u"\u2068" + std::u16string(63, u'A') + u"\u2026\u2069",
            ContainStringForDisplay(name_200));

  // 62 'A's + surrogate pair U+1F600 + 'B' = 65 code units.
  // Truncation at 64 code units avoids splitting the surrogate pair.
  const std::u16string surrogate_name =
      std::u16string(62, u'A') + u"\U0001F600B";
  EXPECT_EQ(u"\u2068" + std::u16string(62, u'A') + u"\u2026\u2069",
            ContainStringForDisplay(surrogate_name));
}

TEST(StringUtilTest, ContainStringDoesNotAlterRtlNames) {
  EXPECT_EQ(u"\u2068\u05de\u05e7\u05dc\u05d3\u05ea\u2069",
            ContainStringForDisplay(u"\u05de\u05e7\u05dc\u05d3\u05ea"));
}

TEST(StringUtilTest, ContainStringIdempotent) {
  const std::u16string name = u"Logitech MX Keys";
  std::u16string once = ContainStringForDisplay(name);
  EXPECT_EQ(once, ContainStringForDisplay(once));

  const std::u16string long_name(70, u'A');
  std::u16string once_long = ContainStringForDisplay(long_name);
  EXPECT_EQ(once_long, ContainStringForDisplay(once_long));

  // Nested/unbalanced isolates remain strictly idempotent.
  const std::u16string nested = u"\u2068Device\u2068Name\u2069";
  std::u16string once_nested = ContainStringForDisplay(nested);
  EXPECT_EQ(once_nested, ContainStringForDisplay(once_nested));
}

TEST(StringUtilTest, ContainStringTruncatesTrailingWhitespaceBeforeEllipsis) {
  const std::u16string name = std::u16string(63, u'A') + u" B";
  EXPECT_EQ(u"\u2068" + std::u16string(63, u'A') + u"\u2026\u2069",
            ContainStringForDisplay(name));
}

TEST(StringUtilTest, ContainStringTruncatesTrailingIsolates) {
  const std::u16string name_with_isolate =
      std::u16string(62, u'A') + u" \u2068BC";
  EXPECT_EQ(u"\u2068" + std::u16string(62, u'A') + u"\u2026\u2069",
            ContainStringForDisplay(name_with_isolate));
}

}  // namespace device
