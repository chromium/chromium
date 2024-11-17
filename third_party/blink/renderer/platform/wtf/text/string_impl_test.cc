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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

TEST(StringImplTest, Create8Bit) {
  scoped_refptr<StringImpl> test_string_impl =
      StringImpl::Create(base::span_from_cstring("1224"));
  EXPECT_TRUE(test_string_impl->Is8Bit());
}

TEST(StringImplTest, Latin1CaseFoldTable) {
  LChar symbol = 0xff;
  while (symbol--) {
    EXPECT_EQ(unicode::FoldCase(symbol),
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

  CaseMap case_map(g_empty_atom);
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

  CaseMap case_map(g_empty_atom);
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

}  // namespace WTF
