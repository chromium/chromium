// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

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
      WTF::AtomicStringTable::Instance().WeakFindLowercase(AtomicString("FOO"));
  EXPECT_FALSE(result.IsNull());

  // This is a particularly icky case; a 16-bit AtomicString that contains
  // only 8-bit data. It can generally only happen if a StringImpl is
  // added directly to the AtomicStringTable.
  String too_wide_string("Foo");
  too_wide_string.Ensure16Bit();
  result = WTF::AtomicStringTable::Instance().WeakFindLowercase(
      AtomicString(too_wide_string.Impl()));
  EXPECT_FALSE(result.IsNull());

  AtomicStringTable::WeakResult result_latin1 =
      WTF::AtomicStringTable::Instance().WeakFindLowercase(
          AtomicString::FromUTF8("Foó"));
  EXPECT_FALSE(result_latin1.IsNull());

  // Only ASCII is lowercased.
  result_latin1 = WTF::AtomicStringTable::Instance().WeakFindLowercase(
      AtomicString::FromUTF8("FoÓ"));
  EXPECT_TRUE(result_latin1.IsNull());

  AtomicStringTable::WeakResult result_unicode =
      WTF::AtomicStringTable::Instance().WeakFindLowercase(
          AtomicString::FromUTF8("foO😀"));
  EXPECT_FALSE(result_unicode.IsNull());

  result_unicode = WTF::AtomicStringTable::Instance().WeakFindLowercase(
      AtomicString::FromUTF8("Goo😀"));
  EXPECT_TRUE(result_unicode.IsNull());
}

}  // namespace WTF
