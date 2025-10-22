/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(StringImplTest, Create8Bit) {
  scoped_refptr<StringImpl> test_string_impl =
      StringImpl::Create(base::span_from_cstring("1224"));
  EXPECT_TRUE(test_string_impl->Is8Bit());
}

TEST(StringImplTest, Latin1CaseFoldTable) {
  LChar symbol = 0xff;
  while (symbol--) {
    EXPECT_EQ(blink::unicode::FoldCase(symbol),
              StringImpl::kLatin1CaseFoldTable[symbol]);
  }
}

TEST(StringImplTest, LowerASCII) {
  scoped_refptr<StringImpl> test_string_impl =
      StringImpl::Create(base::span_from_cstring("link"));
  EXPECT_TRUE(test_string_impl->Is8Bit());
  EXPECT_TRUE(StringImpl::Create(base::span_from_cstring("a\xE1"))->Is8Bit());

  EXPECT_TRUE(Equal(
      test_string_impl.get(),
      StringImpl::Create(base::span_from_cstring("link"))->LowerASCII().get()));
  EXPECT_TRUE(Equal(
      test_string_impl.get(),
      StringImpl::Create(base::span_from_cstring("LINK"))->LowerASCII().get()));
  EXPECT_TRUE(Equal(
      test_string_impl.get(),
      StringImpl::Create(base::span_from_cstring("lInk"))->LowerASCII().get()));

  blink::CaseMap case_map(blink::g_empty_atom);
  EXPECT_TRUE(Equal(
      case_map.ToLower(StringImpl::Create(base::span_from_cstring("LINK")))
          .Impl(),
      StringImpl::Create(base::span_from_cstring("LINK"))->LowerASCII().get()));
  EXPECT_TRUE(Equal(
      case_map.ToLower(StringImpl::Create(base::span_from_cstring("lInk")))
          .Impl(),
      StringImpl::Create(base::span_from_cstring("lInk"))->LowerASCII().get()));

  EXPECT_TRUE(Equal(StringImpl::Create(base::span_from_cstring("a\xE1")).get(),
                    StringImpl::Create(base::span_from_cstring("A\xE1"))
                        ->LowerASCII()
                        .get()));
  EXPECT_TRUE(Equal(StringImpl::Create(base::span_from_cstring("a\xC1")).get(),
                    StringImpl::Create(base::span_from_cstring("A\xC1"))
                        ->LowerASCII()
                        .get()));

  EXPECT_FALSE(Equal(StringImpl::Create(base::span_from_cstring("a\xE1")).get(),
                     StringImpl::Create(base::span_from_cstring("a\xC1"))
                         ->LowerASCII()
                         .get()));
  EXPECT_FALSE(Equal(StringImpl::Create(base::span_from_cstring("A\xE1")).get(),
                     StringImpl::Create(base::span_from_cstring("A\xC1"))
                         ->LowerASCII()
                         .get()));

  static const UChar kTest[4] = {0x006c, 0x0069, 0x006e, 0x006b};  // link
  static const UChar kTestCapitalized[4] = {0x004c, 0x0049, 0x004e,
                                            0x004b};  // LINK

  scoped_refptr<StringImpl> test_string_impl16 = StringImpl::Create(kTest);
  EXPECT_FALSE(test_string_impl16->Is8Bit());

  EXPECT_TRUE(Equal(test_string_impl16.get(),
                    StringImpl::Create(kTest)->LowerASCII().get()));
  EXPECT_TRUE(Equal(test_string_impl16.get(),
                    StringImpl::Create(kTestCapitalized)->LowerASCII().get()));

  static const UChar kTestWithNonASCII[2] = {0x0061, 0x00e1};  // a\xE1
  static const UChar kTestWithNonASCIIComparison[2] = {0x0061,
                                                       0x00c1};  // a\xC1
  static const UChar kTestWithNonASCIICapitalized[2] = {0x0041,
                                                        0x00e1};  // A\xE1

  // Make sure we support scoped_refptr<const StringImpl>.
  scoped_refptr<const StringImpl> const_ref = test_string_impl->IsolatedCopy();
  DCHECK(const_ref->HasOneRef());
  EXPECT_TRUE(Equal(
      StringImpl::Create(kTestWithNonASCII).get(),
      StringImpl::Create(kTestWithNonASCIICapitalized)->LowerASCII().get()));
  EXPECT_FALSE(Equal(
      StringImpl::Create(kTestWithNonASCII).get(),
      StringImpl::Create(kTestWithNonASCIIComparison)->LowerASCII().get()));
}

TEST(StringImplTest, UpperASCII) {
  scoped_refptr<StringImpl> test_string_impl =
      StringImpl::Create(base::span_from_cstring("LINK"));
  EXPECT_TRUE(test_string_impl->Is8Bit());
  EXPECT_TRUE(StringImpl::Create(base::span_from_cstring("a\xE1"))->Is8Bit());

  EXPECT_TRUE(Equal(
      test_string_impl.get(),
      StringImpl::Create(base::span_from_cstring("link"))->UpperASCII().get()));
  EXPECT_TRUE(Equal(
      test_string_impl.get(),
      StringImpl::Create(base::span_from_cstring("LINK"))->UpperASCII().get()));
  EXPECT_TRUE(Equal(
      test_string_impl.get(),
      StringImpl::Create(base::span_from_cstring("lInk"))->UpperASCII().get()));

  blink::CaseMap case_map(blink::g_empty_atom);
  EXPECT_TRUE(Equal(
      case_map.ToUpper(StringImpl::Create(base::span_from_cstring("LINK")))
          .Impl(),
      StringImpl::Create(base::span_from_cstring("LINK"))->UpperASCII().get()));
  EXPECT_TRUE(Equal(
      case_map.ToUpper(StringImpl::Create(base::span_from_cstring("lInk")))
          .Impl(),
      StringImpl::Create(base::span_from_cstring("lInk"))->UpperASCII().get()));

  EXPECT_TRUE(Equal(StringImpl::Create(base::span_from_cstring("A\xE1")).get(),
                    StringImpl::Create(base::span_from_cstring("a\xE1"))
                        ->UpperASCII()
                        .get()));
  EXPECT_TRUE(Equal(StringImpl::Create(base::span_from_cstring("A\xC1")).get(),
                    StringImpl::Create(base::span_from_cstring("a\xC1"))
                        ->UpperASCII()
                        .get()));

  EXPECT_FALSE(Equal(StringImpl::Create(base::span_from_cstring("A\xE1")).get(),
                     StringImpl::Create(base::span_from_cstring("a\xC1"))
                         ->UpperASCII()
                         .get()));
  EXPECT_FALSE(Equal(StringImpl::Create(base::span_from_cstring("A\xE1")).get(),
                     StringImpl::Create(base::span_from_cstring("A\xC1"))
                         ->UpperASCII()
                         .get()));

  static const UChar kTest[4] = {0x006c, 0x0069, 0x006e, 0x006b};  // link
  static const UChar kTestCapitalized[4] = {0x004c, 0x0049, 0x004e,
                                            0x004b};  // LINK

  scoped_refptr<StringImpl> test_string_impl16 =
      StringImpl::Create(kTestCapitalized);
  EXPECT_FALSE(test_string_impl16->Is8Bit());

  EXPECT_TRUE(Equal(test_string_impl16.get(),
                    StringImpl::Create(kTest)->UpperASCII().get()));
  EXPECT_TRUE(Equal(test_string_impl16.get(),
                    StringImpl::Create(kTestCapitalized)->UpperASCII().get()));

  static const UChar kTestWithNonASCII[2] = {0x0061, 0x00e1};  // a\xE1
  static const UChar kTestWithNonASCIIComparison[2] = {0x0061,
                                                       0x00c1};  // a\xC1
  static const UChar kTestWithNonASCIICapitalized[2] = {0x0041,
                                                        0x00e1};  // A\xE1

  // Make sure we support scoped_refptr<const StringImpl>.
  scoped_refptr<const StringImpl> const_ref = test_string_impl->IsolatedCopy();
  DCHECK(const_ref->HasOneRef());
  EXPECT_TRUE(Equal(StringImpl::Create(kTestWithNonASCIICapitalized).get(),
                    StringImpl::Create(kTestWithNonASCII)->UpperASCII().get()));
  EXPECT_FALSE(Equal(
      StringImpl::Create(kTestWithNonASCIICapitalized).get(),
      StringImpl::Create(kTestWithNonASCIIComparison)->UpperASCII().get()));
}

TEST(StringImplTest, CodeUnitCompareIgnoringAsciiCase) {
  StringView lchar1("abC");
  StringView lchar2("ABc");
  StringView lchar3("xyz");
  StringView lchar4("ab");
  StringView uchar1(u"abC");
  StringView uchar2(u"ABc");
  StringView uchar3(u"xyz");
  StringView uchar4(u"ab");
  StringView empty_lchar("");
  StringView empty_uchar(u"");
  StringView lchar_ascii("abc");
  StringView uchar_nonascii(u"ab\u00E1");
  StringView lchar_nonascii("ab\xE1");

  EXPECT_EQ(CodeUnitCompareIgnoringAsciiCase(lchar1, lchar2), 0);
  EXPECT_EQ(CodeUnitCompareIgnoringAsciiCase(uchar1, uchar2), 0);
  EXPECT_EQ(CodeUnitCompareIgnoringAsciiCase(empty_lchar, empty_lchar), 0);
  EXPECT_EQ(CodeUnitCompareIgnoringAsciiCase(empty_uchar, empty_uchar), 0);

  EXPECT_LT(CodeUnitCompareIgnoringAsciiCase(lchar1, lchar3), 0);
  EXPECT_GT(CodeUnitCompareIgnoringAsciiCase(lchar3, lchar1), 0);
  EXPECT_GT(CodeUnitCompareIgnoringAsciiCase(lchar1, lchar4), 0);
  EXPECT_LT(CodeUnitCompareIgnoringAsciiCase(lchar4, lchar1), 0);
  EXPECT_LT(CodeUnitCompareIgnoringAsciiCase(uchar1, uchar3), 0);
  EXPECT_GT(CodeUnitCompareIgnoringAsciiCase(uchar3, uchar1), 0);
  EXPECT_GT(CodeUnitCompareIgnoringAsciiCase(uchar1, uchar4), 0);
  EXPECT_LT(CodeUnitCompareIgnoringAsciiCase(uchar4, uchar1), 0);
  EXPECT_GT(CodeUnitCompareIgnoringAsciiCase(lchar1, empty_lchar), 0);
  EXPECT_LT(CodeUnitCompareIgnoringAsciiCase(empty_lchar, lchar1), 0);

  EXPECT_EQ(CodeUnitCompareIgnoringAsciiCase(lchar1, uchar2), 0);
  EXPECT_EQ(CodeUnitCompareIgnoringAsciiCase(uchar1, lchar2), 0);
  EXPECT_LT(CodeUnitCompareIgnoringAsciiCase(lchar1, uchar3), 0);
  EXPECT_GT(CodeUnitCompareIgnoringAsciiCase(uchar3, lchar1), 0);
  EXPECT_EQ(CodeUnitCompareIgnoringAsciiCase(lchar_ascii, uchar_nonascii), -1);
  EXPECT_EQ(CodeUnitCompareIgnoringAsciiCase(lchar_nonascii, uchar_nonascii),
            0);

  EXPECT_TRUE(CodeUnitCompareIgnoringAsciiCaseLessThan(lchar1, lchar3));
  EXPECT_FALSE(CodeUnitCompareIgnoringAsciiCaseLessThan(lchar3, lchar1));
  EXPECT_FALSE(CodeUnitCompareIgnoringAsciiCaseLessThan(lchar1, lchar2));
  EXPECT_TRUE(CodeUnitCompareIgnoringAsciiCaseLessThan(lchar4, lchar1));
}

TEST(StringImplTest, WtfReverseFind) {
  const auto text = base::byte_span_from_cstring("becde");

  EXPECT_EQ(4u, ReverseFind(text, 'e'));
  EXPECT_EQ(4u, ReverseFind(text, 'e', 4u));
  EXPECT_EQ(1u, ReverseFind(text, 'e', 3u));
  EXPECT_EQ(0u, ReverseFind(text, 'b'));
  EXPECT_EQ(0u, ReverseFind(text, 'b', 0u));

  EXPECT_EQ(kNotFound, ReverseFind(text, 'd', 2u));
}

TEST(StringImplTest, Find) {
  // 8 search and 8 match
  scoped_refptr<StringImpl> test_string_impl =
      StringImpl::Create(base::span_from_cstring("abcde"));
  EXPECT_EQ(0u, test_string_impl->Find("a"));
  EXPECT_EQ(4u, test_string_impl->Find("e"));
  EXPECT_EQ(kNotFound, test_string_impl->Find("z"));
  EXPECT_EQ(3u, test_string_impl->Find("de"));
  EXPECT_EQ(kNotFound, test_string_impl->Find("def"));
  EXPECT_EQ(kNotFound, test_string_impl->Find("abcdef"));
  EXPECT_EQ(2u, test_string_impl->Find("cd"));
  EXPECT_EQ(0u, test_string_impl->Find("abcde"));

  // 8 search and 16 match
  EXPECT_EQ(0u, test_string_impl->Find(u"a"));
  EXPECT_EQ(4u, test_string_impl->Find(u"e"));
  EXPECT_EQ(kNotFound, test_string_impl->Find(u"z"));
  EXPECT_EQ(3u, test_string_impl->Find(u"de"));
  EXPECT_EQ(kNotFound, test_string_impl->Find(u"def"));
  EXPECT_EQ(kNotFound, test_string_impl->Find(u"abcdef"));
  EXPECT_EQ(2u, test_string_impl->Find(u"cd"));
  EXPECT_EQ(0u, test_string_impl->Find(u"abcde"));
  EXPECT_EQ(kNotFound, test_string_impl->Find(u"\U0001F929"));

  // 16 search and 8 match
  scoped_refptr<StringImpl> test_string_impl16 =
      StringImpl::Create(u"abcde\x4100\U0001F929\U0001F926");
  EXPECT_EQ(0u, test_string_impl16->Find("a"));
  EXPECT_EQ(4u, test_string_impl16->Find("e"));
  EXPECT_EQ(kNotFound, test_string_impl16->Find("z"));
  EXPECT_EQ(3u, test_string_impl16->Find("de"));
  EXPECT_EQ(kNotFound, test_string_impl16->Find("def"));
  EXPECT_EQ(kNotFound, test_string_impl16->Find("abcdef"));
  EXPECT_EQ(2u, test_string_impl16->Find("cd"));
  EXPECT_EQ(0u, test_string_impl16->Find("abcde"));

  // 16 search and 16 match
  EXPECT_EQ(0u, test_string_impl16->Find(u"a"));
  EXPECT_EQ(4u, test_string_impl16->Find(u"e"));
  EXPECT_EQ(kNotFound, test_string_impl16->Find(u"z"));
  EXPECT_EQ(3u, test_string_impl16->Find(u"de"));
  EXPECT_EQ(kNotFound, test_string_impl16->Find(u"def"));
  EXPECT_EQ(kNotFound, test_string_impl16->Find(u"abcdef"));
  EXPECT_EQ(2u, test_string_impl16->Find(u"cd"));
  EXPECT_EQ(0u, test_string_impl16->Find(u"abcde"));
  EXPECT_EQ(6u, test_string_impl16->Find(u"\U0001F929"));
  EXPECT_EQ(6u, test_string_impl16->Find(u"\U0001F929\U0001F926"));
  EXPECT_EQ(kNotFound,
            test_string_impl16->Find(u"\U0001F929\U0001F926\U0001F926"));
  EXPECT_EQ(5u, test_string_impl16->Find(u"\x4100"));
}

}  // namespace blink
