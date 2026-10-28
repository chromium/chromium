// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"

#include <array>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(WeakResultTest, BasicOperations) {
  AtomicStringTable::WeakResult null;
  EXPECT_TRUE(null.IsNull());

  EXPECT_TRUE(null == AtomicStringTable::WeakResult());

  AtomicString s("astring");
  AtomicStringTable::WeakResult not_null(s.Impl());
  AtomicStringTable::WeakResult not_null2(s.Impl());

  EXPECT_TRUE(not_null == not_null2);
  EXPECT_FALSE(not_null == null);
  EXPECT_FALSE(not_null.IsNull());

  EXPECT_TRUE(not_null == s);
  EXPECT_TRUE(s == not_null);

  String s2(s);
  EXPECT_TRUE(s2 == not_null);
}

TEST(WeakResultTest, UTF8) {
  AtomicString foo = AtomicString::FromUtf8("foo");
  AtomicString foo_latin1 = AtomicString::FromUtf8("foó");
  AtomicString foo_unicode = AtomicString::FromUtf8("foo😀");

  EXPECT_EQ(foo.length(), 3u);
  EXPECT_EQ(foo_latin1.length(), 3u);
  EXPECT_EQ(foo_unicode.length(), 5u);

  EXPECT_TRUE(foo.Is8Bit());
  EXPECT_TRUE(foo_latin1.Is8Bit());
  EXPECT_FALSE(foo_unicode.Is8Bit());

  EXPECT_EQ(foo.Utf8(), "foo");
  EXPECT_EQ(foo_latin1.Utf8(), "foó");
  EXPECT_EQ(foo_unicode.Utf8(), "foo😀");

  AtomicStringTable::WeakResult result =
      AtomicStringTable::Instance().WeakFindLowercase(AtomicString("FOO"));
  EXPECT_FALSE(result.IsNull());

  // This is a particularly icky case; a 16-bit AtomicString that contains
  // only 8-bit data. It can generally only happen if a StringImpl is
  // added directly to the AtomicStringTable.
  String too_wide_string("Foo");
  too_wide_string.Ensure16Bit();
  result = AtomicStringTable::Instance().WeakFindLowercase(
      AtomicString(too_wide_string.Impl()));
  EXPECT_FALSE(result.IsNull());

  AtomicStringTable::WeakResult result_latin1 =
      AtomicStringTable::Instance().WeakFindLowercase(
          AtomicString::FromUtf8("Foó"));
  EXPECT_FALSE(result_latin1.IsNull());

  // Only ASCII is lowercased.
  result_latin1 = AtomicStringTable::Instance().WeakFindLowercase(
      AtomicString::FromUtf8("FoÓ"));
  EXPECT_TRUE(result_latin1.IsNull());

  AtomicStringTable::WeakResult result_unicode =
      AtomicStringTable::Instance().WeakFindLowercase(
          AtomicString::FromUtf8("foO😀"));
  EXPECT_FALSE(result_unicode.IsNull());

  result_unicode = AtomicStringTable::Instance().WeakFindLowercase(
      AtomicString::FromUtf8("Goo😀"));
  EXPECT_TRUE(result_unicode.IsNull());
}

TEST(AtomicStringTableTest, SmallStringCacheKeepAlive) {
  // Small strings (length <= 16) are kept alive by the SmallStringCache.
  // The cache holds a reference to the string, preventing it from being
  // removed from the table even when all external references are dropped.
  const char* kSmallString = "smcache";
  {
    EXPECT_TRUE(AtomicStringTable::Instance()
                    .WeakFindForTesting(kSmallString)
                    .IsNull());
    AtomicString small(kSmallString);
    EXPECT_FALSE(AtomicStringTable::Instance()
                     .WeakFindForTesting(kSmallString)
                     .IsNull());
    EXPECT_EQ(small.length(), 7u);
    EXPECT_TRUE(small.Is8Bit());
  }
  EXPECT_FALSE(
      AtomicStringTable::Instance().WeakFindForTesting(kSmallString).IsNull());

  // Two distinct 8-byte strings should both be retained in SmallStringCache.
  // Previously, all 8-byte strings hashed to the same slot and evicted each
  // other.
  const char* kEightByte1 = "eightch1";
  const char* kEightByte2 = "eightch2";
  {
    EXPECT_TRUE(
        AtomicStringTable::Instance().WeakFindForTesting(kEightByte1).IsNull());
    EXPECT_TRUE(
        AtomicStringTable::Instance().WeakFindForTesting(kEightByte2).IsNull());
    AtomicString eight1(kEightByte1);
    AtomicString eight2(kEightByte2);
    EXPECT_EQ(eight1.length(), 8u);
    EXPECT_EQ(eight2.length(), 8u);
    EXPECT_FALSE(
        AtomicStringTable::Instance().WeakFindForTesting(kEightByte1).IsNull());
    EXPECT_FALSE(
        AtomicStringTable::Instance().WeakFindForTesting(kEightByte2).IsNull());
  }
  EXPECT_FALSE(
      AtomicStringTable::Instance().WeakFindForTesting(kEightByte1).IsNull());
  EXPECT_FALSE(
      AtomicStringTable::Instance().WeakFindForTesting(kEightByte2).IsNull());

  // Large strings (length > 16) are NOT kept alive by the SmallStringCache.
  const char* kLargeString = "largenotcachedstring17";
  {
    EXPECT_TRUE(AtomicStringTable::Instance()
                    .WeakFindForTesting(kLargeString)
                    .IsNull());
    AtomicString large(kLargeString);
    EXPECT_FALSE(AtomicStringTable::Instance()
                     .WeakFindForTesting(kLargeString)
                     .IsNull());
    EXPECT_GT(large.length(), 16u);
  }
  // After destroying 'large', it should be removed from the table.
  EXPECT_TRUE(
      AtomicStringTable::Instance().WeakFindForTesting(kLargeString).IsNull());
}

TEST(AtomicStringTableTest, EmbeddedNulls) {
  // Small strings (<= 16 chars) are stored in the SmallStringCache.
  // They must correctly handle embedded nulls and not confuse them with
  // shorter strings or other strings that might hash similarly.

  AtomicString ltr("ltr");
  const LChar kLtrNull[] = {'l', 't', 'r', '\0'};
  AtomicString ltr_with_null{base::span<const LChar>(kLtrNull)};
  const LChar kLtrNullA[] = {'l', 't', 'r', '\0', 'a'};
  AtomicString ltr_with_null_a{base::span<const LChar>(kLtrNullA)};

  EXPECT_EQ(ltr.length(), 3u);
  EXPECT_EQ(ltr_with_null.length(), 4u);
  EXPECT_NE(ltr, ltr_with_null);
  EXPECT_EQ(ltr_with_null[3], '\0');
  EXPECT_EQ(ltr_with_null_a.length(), 5u);
  EXPECT_NE(ltr_with_null, ltr_with_null_a);
  EXPECT_EQ(ltr_with_null_a[3], '\0');
  EXPECT_EQ(ltr_with_null_a[4], 'a');

  const LChar kNull[] = {'\0'};
  AtomicString null_string{base::span<const LChar>(kNull)};

  EXPECT_EQ(null_string.length(), 1u);
  EXPECT_FALSE(null_string.empty());
  EXPECT_NE(null_string, g_empty_atom);
  EXPECT_EQ(null_string[0], '\0');
}

TEST(AtomicStringTableTest, WideSmallStringSignatures) {
  const std::array<LChar, 16> latin1 = {0,    'a',  0x7f, 0x80, 0xff, 'b',
                                        0xe9, 'c',  0x81, 'd',  0xfe, 'e',
                                        'f',  0x90, 'g',  'h'};
  alignas(16) std::array<UChar, 23> storage;
  for (wtf_size_t offset = 0; offset < 8; ++offset) {
    SCOPED_TRACE(offset);
    auto wide = base::span(storage).subspan(offset).first<16>();
    for (wtf_size_t i = 0; i < latin1.size(); ++i) {
      wide[i] = latin1[i];
    }
    for (wtf_size_t length = 1; length <= latin1.size(); ++length) {
      SCOPED_TRACE(length);
      const auto narrow_chars = base::span(latin1).first(length);
      const auto wide_chars = wide.first(length);
      AtomicString narrow(narrow_chars);
      EXPECT_EQ(narrow.Impl(), AtomicString(wide_chars).Impl());
      EXPECT_EQ(
          narrow.Impl(),
          AtomicString(wide_chars, AtomicStringUCharEncoding::kIs8Bit).Impl());
      for (wtf_size_t i = 0; i < length; ++i) {
        SCOPED_TRACE(i);
        const UChar original = wide[i];
        wide[i] = original ^ 1;
        AtomicString changed(wide_chars);
        EXPECT_NE(narrow, changed);
        EXPECT_EQ(wide[i], changed[i]);
        wide[i] = original | 0x100;
        AtomicString unicode(wide_chars);
        EXPECT_NE(narrow, unicode);
        EXPECT_FALSE(unicode.Is8Bit());
        EXPECT_EQ(wide[i], unicode[i]);
        wide[i] = original;
        EXPECT_EQ(narrow.Impl(), AtomicString(wide_chars).Impl());
      }
    }
  }
}

}  // namespace blink
