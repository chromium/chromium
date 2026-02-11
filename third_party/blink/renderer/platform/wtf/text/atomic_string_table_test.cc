// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"

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
  AtomicString foo = AtomicString::FromUTF8("foo");
  AtomicString foo_latin1 = AtomicString::FromUTF8("foó");
  AtomicString foo_unicode = AtomicString::FromUTF8("foo😀");

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
          AtomicString::FromUTF8("Foó"));
  EXPECT_FALSE(result_latin1.IsNull());

  // Only ASCII is lowercased.
  result_latin1 = AtomicStringTable::Instance().WeakFindLowercase(
      AtomicString::FromUTF8("FoÓ"));
  EXPECT_TRUE(result_latin1.IsNull());

  AtomicStringTable::WeakResult result_unicode =
      AtomicStringTable::Instance().WeakFindLowercase(
          AtomicString::FromUTF8("foO😀"));
  EXPECT_FALSE(result_unicode.IsNull());

  result_unicode = AtomicStringTable::Instance().WeakFindLowercase(
      AtomicString::FromUTF8("Goo😀"));
  EXPECT_TRUE(result_unicode.IsNull());
}

TEST(AtomicStringTableTest, SmallStringCacheKeepAlive) {
  // Small strings (length <= 7) are kept alive by the SmallStringCache.
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

  // Large strings (length > 7) are NOT kept alive by the SmallStringCache.
  const char* kLargeString = "largenotcached";
  {
    EXPECT_TRUE(AtomicStringTable::Instance()
                    .WeakFindForTesting(kLargeString)
                    .IsNull());
    AtomicString large(kLargeString);
    EXPECT_FALSE(AtomicStringTable::Instance()
                     .WeakFindForTesting(kLargeString)
                     .IsNull());
    EXPECT_GT(large.length(), 7u);
  }
  // After destroying 'large', it should be removed from the table.
  EXPECT_TRUE(
      AtomicStringTable::Instance().WeakFindForTesting(kLargeString).IsNull());
}

TEST(AtomicStringTableTest, EmbeddedNulls) {
  // Small strings (<= 7 chars) are stored in the SmallStringCache.
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

}  // namespace blink
