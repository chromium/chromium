/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

namespace {

void ExpectBuilderContent(const String& expected,
                          const StringBuilder& builder) {
  // Not using builder.toString() because it changes internal state of builder.
  if (builder.Is8Bit())
    EXPECT_EQ(expected, String(builder.Characters8(), builder.length()));
  else
    EXPECT_EQ(expected, String(builder.Characters16(), builder.length()));
}

void ExpectEmpty(const StringBuilder& builder) {
  EXPECT_EQ(0U, builder.length());
  EXPECT_TRUE(builder.IsEmpty());
  EXPECT_EQ(nullptr, builder.Characters8());
}

}  // namespace

TEST(StringBuilderTest, DefaultConstructor) {
  StringBuilder builder;
  ExpectEmpty(builder);
}

TEST(StringBuilderTest, Append) {
  StringBuilder builder;
  builder.Append(String("0123456789"));
  ExpectBuilderContent("0123456789", builder);
  builder.Append("abcd");
  ExpectBuilderContent("0123456789abcd", builder);
  builder.Append("efgh", 3);
  ExpectBuilderContent("0123456789abcdefg", builder);
  builder.Append("");
  ExpectBuilderContent("0123456789abcdefg", builder);
  builder.Append('#');
  ExpectBuilderContent("0123456789abcdefg#", builder);

  builder.ToString();  // Test after reifyString().
  StringBuilder builder1;
  builder.Append("", 0);
  ExpectBuilderContent("0123456789abcdefg#", builder);
  builder1.Append(builder.Characters8(), builder.length());
  builder1.Append("XYZ");
  builder.Append(builder1.Characters8(), builder1.length());
  ExpectBuilderContent("0123456789abcdefg#0123456789abcdefg#XYZ", builder);

  StringBuilder builder2;
  builder2.ReserveCapacity(100);
  builder2.Append("xyz");
  const LChar* characters = builder2.Characters8();
  builder2.Append("0123456789");
  EXPECT_EQ(characters, builder2.Characters8());

  StringBuilder builder3;
  builder3.Append("xyz", 1, 2);
  ExpectBuilderContent("yz", builder3);

  StringBuilder builder4;
  builder4.Append("abc", 5, 3);
  ExpectEmpty(builder4);

  StringBuilder builder5;
  builder5.Append(StringView(StringView("def"), 1, 1));
  ExpectBuilderContent("e", builder5);

  // append() has special code paths for String backed StringView instead of
  // just char* backed ones.
  StringBuilder builder6;
  builder6.Append(String("ghi"), 1, 2);
  ExpectBuilderContent("hi", builder6);

  // Test appending UChar32 characters to StringBuilder.
  StringBuilder builder_for_u_char32_append;
  UChar32 fraktur_a_char = 0x1D504;
  // The fraktur A is not in the BMP, so it's two UTF-16 code units long.
  builder_for_u_char32_append.Append(fraktur_a_char);
  EXPECT_FALSE(builder_for_u_char32_append.Is8Bit());
  EXPECT_EQ(2U, builder_for_u_char32_append.length());
  builder_for_u_char32_append.Append(static_cast<UChar32>('A'));
  EXPECT_EQ(3U, builder_for_u_char32_append.length());
  const UChar result_array[] = {U16_LEAD(fraktur_a_char),
                                U16_TRAIL(fraktur_a_char), 'A'};
  ExpectBuilderContent(String(result_array, base::size(result_array)),
                       builder_for_u_char32_append);
}

TEST(StringBuilderTest, AppendSharingImpl) {
  String string("abc");
  StringBuilder builder1;
  builder1.Append(string);
  EXPECT_EQ(string.Impl(), builder1.ToString().Impl());
  EXPECT_EQ(string.Impl(), builder1.ToAtomicString().Impl());

  StringBuilder builder2;
  builder2.Append(string, 0, string.length());
  EXPECT_EQ(string.Impl(), builder2.ToString().Impl());
  EXPECT_EQ(string.Impl(), builder2.ToAtomicString().Impl());
}

TEST(StringBuilderTest, ToString) {
  StringBuilder builder;
  builder.Append("0123456789");
  String string = builder.ToString();
  EXPECT_EQ(String("0123456789"), string);
  EXPECT_EQ(string.Impl(), builder.ToString().Impl());

  // Changing the StringBuilder should not affect the original result of
  // toString().
  builder.Append("abcdefghijklmnopqrstuvwxyz");
  EXPECT_EQ(String("0123456789"), string);

  // Changing the StringBuilder should not affect the original result of
  // toString() in case the capacity is not changed.
  builder.ReserveCapacity(200);
  string = builder.ToString();
  EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyz"), string);
  builder.Append("ABC");
  EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyz"), string);

  // Changing the original result of toString() should not affect the content of
  // the StringBuilder.
  String string1 = builder.ToString();
  EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABC"), string1);
  string1 = string1 + "DEF";
  EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABC"),
            builder.ToString());
  EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABCDEF"), string1);

  // Resizing the StringBuilder should not affect the original result of
  // toString().
  string1 = builder.ToString();
  builder.Resize(10);
  builder.Append("###");
  EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABC"), string1);
}

TEST(StringBuilderTest, Clear) {
  StringBuilder builder;
  builder.Append("0123456789");
  builder.Clear();
  ExpectEmpty(builder);
}

TEST(StringBuilderTest, Array) {
  StringBuilder builder;
  builder.Append("0123456789");
  EXPECT_EQ('0', static_cast<char>(builder[0]));
  EXPECT_EQ('9', static_cast<char>(builder[9]));
  builder.ToString();  // Test after reifyString().
  EXPECT_EQ('0', static_cast<char>(builder[0]));
  EXPECT_EQ('9', static_cast<char>(builder[9]));
}

TEST(StringBuilderTest, Resize) {
  StringBuilder builder;
  builder.Append("0123456789");
  builder.Resize(10);
  EXPECT_EQ(10U, builder.length());
  ExpectBuilderContent("0123456789", builder);
  builder.Resize(8);
  EXPECT_EQ(8U, builder.length());
  ExpectBuilderContent("01234567", builder);

  builder.ToString();
  builder.Resize(7);
  EXPECT_EQ(7U, builder.length());
  ExpectBuilderContent("0123456", builder);
  builder.Resize(0);
  ExpectEmpty(builder);
}

TEST(StringBuilderTest, Erase) {
  StringBuilder builder;
  builder.Append(String("01234"));
  // Erase from String.
  builder.erase(3);
  ExpectBuilderContent("0124", builder);
  // Erase from buffer.
  builder.erase(1);
  ExpectBuilderContent("024", builder);
}

TEST(StringBuilderTest, Erase16) {
  StringBuilder builder;
  builder.Append(String(u"\uFF10\uFF11\uFF12\uFF13\uFF14"));
  // Erase from String.
  builder.erase(3);
  ExpectBuilderContent(u"\uFF10\uFF11\uFF12\uFF14", builder);
  // Erase from buffer.
  builder.erase(1);
  ExpectBuilderContent(u"\uFF10\uFF12\uFF14", builder);
}

TEST(StringBuilderTest, EraseLast) {
  StringBuilder builder;
  builder.Append("01234");
  builder.erase(4);
  ExpectBuilderContent("0123", builder);
}

TEST(StringBuilderTest, Equal) {
  StringBuilder builder1;
  StringBuilder builder2;
  EXPECT_TRUE(builder1 == builder2);
  EXPECT_TRUE(Equal(builder1, static_cast<LChar*>(nullptr), 0));
  EXPECT_TRUE(builder1 == String());
  EXPECT_TRUE(String() == builder1);
  EXPECT_TRUE(builder1 != String("abc"));

  builder1.Append("123");
  builder1.ReserveCapacity(32);
  builder2.Append("123");
  builder1.ReserveCapacity(64);
  EXPECT_TRUE(builder1 == builder2);
  EXPECT_TRUE(builder1 == String("123"));
  EXPECT_TRUE(String("123") == builder1);

  builder2.Append("456");
  EXPECT_TRUE(builder1 != builder2);
  EXPECT_TRUE(builder2 != builder1);
  EXPECT_TRUE(String("123") != builder2);
  EXPECT_TRUE(builder2 != String("123"));
  builder2.ToString();  // Test after reifyString().
  EXPECT_TRUE(builder1 != builder2);

  builder2.Resize(3);
  EXPECT_TRUE(builder1 == builder2);

  builder1.ToString();  // Test after reifyString().
  EXPECT_TRUE(builder1 == builder2);
}

TEST(StringBuilderTest, ToAtomicString) {
  StringBuilder builder;
  builder.Append("123");
  AtomicString atomic_string = builder.ToAtomicString();
  EXPECT_EQ(String("123"), atomic_string);

  builder.ReserveCapacity(256);
  for (int i = builder.length(); i < 128; i++)
    builder.Append('x');
  AtomicString atomic_string1 = builder.ToAtomicString();
  EXPECT_EQ(128u, atomic_string1.length());
  EXPECT_EQ('x', atomic_string1[127]);

  // Later change of builder should not affect the atomic string.
  for (int i = builder.length(); i < 256; i++)
    builder.Append('x');
  EXPECT_EQ(128u, atomic_string1.length());

  String string = builder.ToString();
  AtomicString atomic_string2 = builder.ToAtomicString();
  // They should share the same StringImpl.
  EXPECT_EQ(atomic_string2.Impl(), string.Impl());
}

TEST(StringBuilderTest, ToAtomicStringOnEmpty) {
  {  // Default constructed.
    StringBuilder builder;
    AtomicString atomic_string = builder.ToAtomicString();
    EXPECT_EQ(g_empty_atom, atomic_string);
  }
  {  // With capacity.
    StringBuilder builder;
    builder.ReserveCapacity(64);
    AtomicString atomic_string = builder.ToAtomicString();
    EXPECT_EQ(g_empty_atom, atomic_string);
  }
  {  // AtomicString constructed from a null string.
    StringBuilder builder;
    builder.Append(String());
    AtomicString atomic_string = builder.ToAtomicString();
    EXPECT_EQ(g_empty_atom, atomic_string);
  }
  {  // AtomicString constructed from an empty string.
    StringBuilder builder;
    builder.Append(g_empty_string);
    AtomicString atomic_string = builder.ToAtomicString();
    EXPECT_EQ(g_empty_atom, atomic_string);
  }
  {  // AtomicString constructed from an empty StringBuilder.
    StringBuilder builder;
    StringBuilder empty_builder;
    builder.Append(empty_builder);
    AtomicString atomic_string = builder.ToAtomicString();
    EXPECT_EQ(g_empty_atom, atomic_string);
  }
  {  // AtomicString constructed from an empty char* string.
    StringBuilder builder;
    builder.Append("", 0);
    AtomicString atomic_string = builder.ToAtomicString();
    EXPECT_EQ(g_empty_atom, atomic_string);
  }
  {  // Cleared StringBuilder.
    StringBuilder builder;
    builder.Append("WebKit");
    builder.Clear();
    AtomicString atomic_string = builder.ToAtomicString();
    EXPECT_EQ(g_empty_atom, atomic_string);
  }
}

TEST(StringBuilderTest, Substring) {
  {  // Default constructed.
    StringBuilder builder;
    String substring = builder.Substring(0, 10);
    EXPECT_EQ(g_empty_string, substring);
  }
  {  // With capacity.
    StringBuilder builder;
    builder.ReserveCapacity(64);
    builder.Append("abc");
    String substring = builder.Substring(2, 10);
    EXPECT_EQ(String("c"), substring);
  }
}

TEST(StringBuilderTest, AppendNumberDoubleUChar) {
  const double kSomeNumber = 1.2345;
  StringBuilder reference;
  reference.Append(kReplacementCharacter);  // Make it UTF-16.
  reference.Append(String::Number(kSomeNumber));
  StringBuilder test;
  test.Append(kReplacementCharacter);
  test.AppendNumber(kSomeNumber);
  EXPECT_EQ(reference, test);
}

}  // namespace WTF
