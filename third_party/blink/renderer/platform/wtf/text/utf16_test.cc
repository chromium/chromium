// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/utf16.h"

#include <limits>
#include <vector>

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(Utf16Test, ContainsOnlyLatin1) {
  // Empty string.
  std::vector<UChar> utf16_str = {};
  EXPECT_TRUE(ContainsOnlyLatin1(utf16_str));

  // All ASCII.
  utf16_str = {'a', 'b', 'c'};
  EXPECT_TRUE(ContainsOnlyLatin1(utf16_str));

  // Mixed ASCII and Latin1.
  utf16_str = {'a', 'b', 'c', u'¨'};
  EXPECT_TRUE(ContainsOnlyLatin1(utf16_str));

  // Mixed ASCII, Latin1, and non-lati1
  utf16_str = {'a', 'a', 'a', 'a', u'€', u'¨'};
  EXPECT_FALSE(ContainsOnlyLatin1(utf16_str));

  // Try a bunch of strings with a character (some latin, some not) in a random
  // location in a string.
  for (UChar i = 0; i <= 0x02FF; ++i) {
    // Put the character at a random point in a string.
    bool is_latin = !(i & 0xFF00);
    std::vector<UChar> str(65, 'a');

    // Set a random value in the vector to i.
    int position = base::RandInt(1, 64);
    str[position] = i;
    SCOPED_TRACE(base::StringPrintf("Char: %u Position: %d", i, position));

    // Aligned string
    EXPECT_EQ(is_latin, ContainsOnlyLatin1(str));
    // Offset string
    base::span<const UChar> str_span(str);
    EXPECT_EQ(is_latin, ContainsOnlyLatin1(str_span.subspan(1u)));
  }
}

}  // namespace blink
