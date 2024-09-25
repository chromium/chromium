// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

const char kChars[] = "12345";
const char16_t kCharsU[] = u"12345";
const LChar* const kChars8 = reinterpret_cast<const LChar*>(kChars);
const UChar* const kChars16 = reinterpret_cast<const UChar*>(kCharsU);

TEST(StringViewTest, ConstructionStringImpl8) {
  scoped_refptr<StringImpl> impl8_bit = StringImpl::Create(kChars8, 5);

  // StringView(StringImpl*);
  ASSERT_TRUE(StringView(impl8_bit.get()).Is8Bit());
  EXPECT_FALSE(StringView(impl8_bit.get()).IsNull());
  EXPECT_EQ(impl8_bit->Characters8(),
            StringView(impl8_bit.get()).Characters8());
  EXPECT_EQ(impl8_bit->length(), StringView(impl8_bit.get()).length());
  EXPECT_EQ(kChars, StringView(impl8_bit.get()));

  // StringView(StringImpl*, unsigned offset);
  ASSERT_TRUE(StringView(impl8_bit.get(), 2).Is8Bit());
  EXPECT_FALSE(StringView(impl8_bit.get(), 2).IsNull());
  EXPECT_EQ(impl8_bit->Characters8() + 2,
            StringView(impl8_bit.get(), 2).Characters8());
  EXPECT_EQ(3u, StringView(impl8_bit.get(), 2).length());
  EXPECT_EQ(StringView("345"), StringView(impl8_bit.get(), 2));
  EXPECT_EQ("345", StringView(impl8_bit.get(), 2));

  // StringView(StringImpl*, unsigned offset, unsigned length);
  ASSERT_TRUE(StringView(impl8_bit.get(), 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(impl8_bit.get(), 2, 1).IsNull());
  EXPECT_EQ(impl8_bit->Characters8() + 2,
            StringView(impl8_bit.get(), 2, 1).Characters8());
  EXPECT_EQ(1u, StringView(impl8_bit.get(), 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(impl8_bit.get(), 2, 1));
  EXPECT_EQ("3", StringView(impl8_bit.get(), 2, 1));
}

TEST(StringViewTest, ConstructionStringImpl16) {
  scoped_refptr<StringImpl> impl16_bit = StringImpl::Create(kChars16, 5);

  // StringView(StringImpl*);
  ASSERT_FALSE(StringView(impl16_bit.get()).Is8Bit());
  EXPECT_FALSE(StringView(impl16_bit.get()).IsNull());
  EXPECT_EQ(impl16_bit->Characters16(),
            StringView(impl16_bit.get()).Characters16());
  EXPECT_EQ(impl16_bit->length(), StringView(impl16_bit.get()).length());
  EXPECT_EQ(kChars, StringView(impl16_bit.get()));

  // StringView(StringImpl*, unsigned offset);
  ASSERT_FALSE(StringView(impl16_bit.get(), 2).Is8Bit());
  EXPECT_FALSE(StringView(impl16_bit.get(), 2).IsNull());
  EXPECT_EQ(impl16_bit->Characters16() + 2,
            StringView(impl16_bit.get(), 2).Characters16());
  EXPECT_EQ(3u, StringView(impl16_bit.get(), 2).length());
  EXPECT_EQ(StringView("345"), StringView(impl16_bit.get(), 2));
  EXPECT_EQ("345", StringView(impl16_bit.get(), 2));

  // StringView(StringImpl*, unsigned offset, unsigned length);
  ASSERT_FALSE(StringView(impl16_bit.get(), 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(impl16_bit.get(), 2, 1).IsNull());
  EXPECT_EQ(impl16_bit->Characters16() + 2,
            StringView(impl16_bit.get(), 2, 1).Characters16());
  EXPECT_EQ(1u, StringView(impl16_bit.get(), 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(impl16_bit.get(), 2, 1));
  EXPECT_EQ("3", StringView(impl16_bit.get(), 2, 1));
}

TEST(StringViewTest, ConstructionStringImplRef8) {
  scoped_refptr<StringImpl> impl8_bit = StringImpl::Create(kChars8, 5);

  // StringView(StringImpl&);
  ASSERT_TRUE(StringView(*impl8_bit).Is8Bit());
  EXPECT_FALSE(StringView(*impl8_bit).IsNull());
  EXPECT_EQ(impl8_bit->Characters8(), StringView(*impl8_bit).Characters8());
  EXPECT_EQ(impl8_bit->length(), StringView(*impl8_bit).length());
  EXPECT_EQ(kChars, StringView(*impl8_bit));

  // StringView(StringImpl&, unsigned offset);
  ASSERT_TRUE(StringView(*impl8_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(*impl8_bit, 2).IsNull());
  EXPECT_EQ(impl8_bit->Characters8() + 2,
            StringView(*impl8_bit, 2).Characters8());
  EXPECT_EQ(3u, StringView(*impl8_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(*impl8_bit, 2));
  EXPECT_EQ("345", StringView(*impl8_bit, 2));

  // StringView(StringImpl&, unsigned offset, unsigned length);
  ASSERT_TRUE(StringView(*impl8_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(*impl8_bit, 2, 1).IsNull());
  EXPECT_EQ(impl8_bit->Characters8() + 2,
            StringView(*impl8_bit, 2, 1).Characters8());
  EXPECT_EQ(1u, StringView(*impl8_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(*impl8_bit, 2, 1));
  EXPECT_EQ("3", StringView(*impl8_bit, 2, 1));
}

TEST(StringViewTest, ConstructionStringImplRef16) {
  scoped_refptr<StringImpl> impl16_bit = StringImpl::Create(kChars16, 5);

  // StringView(StringImpl&);
  ASSERT_FALSE(StringView(*impl16_bit).Is8Bit());
  EXPECT_FALSE(StringView(*impl16_bit).IsNull());
  EXPECT_EQ(impl16_bit->Characters16(), StringView(*impl16_bit).Characters16());
  EXPECT_EQ(impl16_bit->length(), StringView(*impl16_bit).length());
  EXPECT_EQ(kChars, StringView(*impl16_bit));

  // StringView(StringImpl&, unsigned offset);
  ASSERT_FALSE(StringView(*impl16_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(*impl16_bit, 2).IsNull());
  EXPECT_EQ(impl16_bit->Characters16() + 2,
            StringView(*impl16_bit, 2).Characters16());
  EXPECT_EQ(3u, StringView(*impl16_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(*impl16_bit, 2));
  EXPECT_EQ("345", StringView(*impl16_bit, 2));

  // StringView(StringImpl&, unsigned offset, unsigned length);
  ASSERT_FALSE(StringView(*impl16_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(*impl16_bit, 2, 1).IsNull());
  EXPECT_EQ(impl16_bit->Characters16() + 2,
            StringView(*impl16_bit, 2, 1).Characters16());
  EXPECT_EQ(1u, StringView(*impl16_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(*impl16_bit, 2, 1));
  EXPECT_EQ("3", StringView(*impl16_bit, 2, 1));
}

TEST(StringViewTest, ConstructionString8) {
  String string8_bit = String(StringImpl::Create(kChars8, 5));

  // StringView(const String&);
  ASSERT_TRUE(StringView(string8_bit).Is8Bit());
  EXPECT_FALSE(StringView(string8_bit).IsNull());
  EXPECT_EQ(string8_bit.Characters8(), StringView(string8_bit).Characters8());
  EXPECT_EQ(string8_bit.length(), StringView(string8_bit).length());
  EXPECT_EQ(kChars, StringView(string8_bit));

  // StringView(const String&, unsigned offset);
  ASSERT_TRUE(StringView(string8_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(string8_bit, 2).IsNull());
  EXPECT_EQ(string8_bit.Characters8() + 2,
            StringView(string8_bit, 2).Characters8());
  EXPECT_EQ(3u, StringView(string8_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(string8_bit, 2));
  EXPECT_EQ("345", StringView(string8_bit, 2));

  // StringView(const String&, unsigned offset, unsigned length);
  ASSERT_TRUE(StringView(string8_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(string8_bit, 2, 1).IsNull());
  EXPECT_EQ(string8_bit.Characters8() + 2,
            StringView(string8_bit, 2, 1).Characters8());
  EXPECT_EQ(1u, StringView(string8_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(string8_bit, 2, 1));
  EXPECT_EQ("3", StringView(string8_bit, 2, 1));
}

TEST(StringViewTest, ConstructionString16) {
  String string16_bit = String(StringImpl::Create(kChars16, 5));

  // StringView(const String&);
  ASSERT_FALSE(StringView(string16_bit).Is8Bit());
  EXPECT_FALSE(StringView(string16_bit).IsNull());
  EXPECT_EQ(string16_bit.Characters16(),
            StringView(string16_bit).Characters16());
  EXPECT_EQ(string16_bit.length(), StringView(string16_bit).length());
  EXPECT_EQ(kChars, StringView(string16_bit));

  // StringView(const String&, unsigned offset);
  ASSERT_FALSE(StringView(string16_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(string16_bit, 2).IsNull());
  EXPECT_EQ(string16_bit.Characters16() + 2,
            StringView(string16_bit, 2).Characters16());
  EXPECT_EQ(3u, StringView(string16_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(string16_bit, 2));
  EXPECT_EQ("345", StringView(string16_bit, 2));

  // StringView(const String&, unsigned offset, unsigned length);
  ASSERT_FALSE(StringView(string16_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(string16_bit, 2, 1).IsNull());
  EXPECT_EQ(string16_bit.Characters16() + 2,
            StringView(string16_bit, 2, 1).Characters16());
  EXPECT_EQ(1u, StringView(string16_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(string16_bit, 2, 1));
  EXPECT_EQ("3", StringView(string16_bit, 2, 1));
}

TEST(StringViewTest, ConstructionAtomicString8) {
  AtomicString atom8_bit = AtomicString(StringImpl::Create(kChars8, 5));

  // StringView(const AtomicString&);
  ASSERT_TRUE(StringView(atom8_bit).Is8Bit());
  EXPECT_FALSE(StringView(atom8_bit).IsNull());
  EXPECT_EQ(atom8_bit.Characters8(), StringView(atom8_bit).Characters8());
  EXPECT_EQ(atom8_bit.length(), StringView(atom8_bit).length());
  EXPECT_EQ(kChars, StringView(atom8_bit));

  // StringView(const AtomicString&, unsigned offset);
  ASSERT_TRUE(StringView(atom8_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(atom8_bit, 2).IsNull());
  EXPECT_EQ(atom8_bit.Characters8() + 2,
            StringView(atom8_bit, 2).Characters8());
  EXPECT_EQ(3u, StringView(atom8_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(atom8_bit, 2));
  EXPECT_EQ("345", StringView(atom8_bit, 2));

  // StringView(const AtomicString&, unsigned offset, unsigned length);
  ASSERT_TRUE(StringView(atom8_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(atom8_bit, 2, 1).IsNull());
  EXPECT_EQ(atom8_bit.Characters8() + 2,
            StringView(atom8_bit, 2, 1).Characters8());
  EXPECT_EQ(1u, StringView(atom8_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(atom8_bit, 2, 1));
  EXPECT_EQ("3", StringView(atom8_bit, 2, 1));
}

TEST(StringViewTest, ConstructionAtomicString16) {
  AtomicString atom16_bit = AtomicString(StringImpl::Create(kChars16, 5));

  // StringView(const AtomicString&);
  ASSERT_FALSE(StringView(atom16_bit).Is8Bit());
  EXPECT_FALSE(StringView(atom16_bit).IsNull());
  EXPECT_EQ(atom16_bit.Characters16(), StringView(atom16_bit).Characters16());
  EXPECT_EQ(atom16_bit.length(), StringView(atom16_bit).length());
  EXPECT_EQ(kChars, StringView(atom16_bit));

  // StringView(const AtomicString&, unsigned offset);
  ASSERT_FALSE(StringView(atom16_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(atom16_bit, 2).IsNull());
  EXPECT_EQ(atom16_bit.Characters16() + 2,
            StringView(atom16_bit, 2).Characters16());
  EXPECT_EQ(3u, StringView(atom16_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(atom16_bit, 2));
  EXPECT_EQ("345", StringView(atom16_bit, 2));

  // StringView(const AtomicString&, unsigned offset, unsigned length);
  ASSERT_FALSE(StringView(atom16_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(atom16_bit, 2, 1).IsNull());
  EXPECT_EQ(atom16_bit.Characters16() + 2,
            StringView(atom16_bit, 2, 1).Characters16());
  EXPECT_EQ(1u, StringView(atom16_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(atom16_bit, 2, 1));
  EXPECT_EQ("3", StringView(atom16_bit, 2, 1));
}

TEST(StringViewTest, ConstructionStringView8) {
  StringView view8_bit = StringView(kChars8, 5u);

  // StringView(StringView&);
  ASSERT_TRUE(StringView(view8_bit).Is8Bit());
  EXPECT_FALSE(StringView(view8_bit).IsNull());
  EXPECT_EQ(view8_bit.Characters8(), StringView(view8_bit).Characters8());
  EXPECT_EQ(view8_bit.length(), StringView(view8_bit).length());
  EXPECT_EQ(kChars, StringView(view8_bit));

  // StringView(const StringView&, unsigned offset);
  ASSERT_TRUE(StringView(view8_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(view8_bit, 2).IsNull());
  EXPECT_EQ(view8_bit.Characters8() + 2,
            StringView(view8_bit, 2).Characters8());
  EXPECT_EQ(3u, StringView(view8_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(view8_bit, 2));
  EXPECT_EQ("345", StringView(view8_bit, 2));

  // StringView(const StringView&, unsigned offset, unsigned length);
  ASSERT_TRUE(StringView(view8_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(view8_bit, 2, 1).IsNull());
  EXPECT_EQ(view8_bit.Characters8() + 2,
            StringView(view8_bit, 2, 1).Characters8());
  EXPECT_EQ(1u, StringView(view8_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(view8_bit, 2, 1));
  EXPECT_EQ("3", StringView(view8_bit, 2, 1));
}

TEST(StringViewTest, ConstructionStringView16) {
  StringView view16_bit = StringView(kChars16, 5);

  // StringView(StringView&);
  ASSERT_FALSE(StringView(view16_bit).Is8Bit());
  EXPECT_FALSE(StringView(view16_bit).IsNull());
  EXPECT_EQ(view16_bit.Characters16(), StringView(view16_bit).Characters16());
  EXPECT_EQ(view16_bit.length(), StringView(view16_bit).length());
  EXPECT_EQ(kChars, StringView(view16_bit));

  // StringView(const StringView&, unsigned offset);
  ASSERT_FALSE(StringView(view16_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(view16_bit, 2).IsNull());
  EXPECT_EQ(view16_bit.Characters16() + 2,
            StringView(view16_bit, 2).Characters16());
  EXPECT_EQ(3u, StringView(view16_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(view16_bit, 2));
  EXPECT_EQ("345", StringView(view16_bit, 2));

  // StringView(const StringView&, unsigned offset, unsigned length);
  ASSERT_FALSE(StringView(view16_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(view16_bit, 2, 1).IsNull());
  EXPECT_EQ(view16_bit.Characters16() + 2,
            StringView(view16_bit, 2, 1).Characters16());
  EXPECT_EQ(1u, StringView(view16_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(view16_bit, 2, 1));
  EXPECT_EQ("3", StringView(view16_bit, 2, 1));
}

TEST(StringViewTest, SubstringContainsOnlyWhitespaceOrEmpty) {
  EXPECT_TRUE(StringView("  ").SubstringContainsOnlyWhitespaceOrEmpty(0, 1));
  EXPECT_TRUE(StringView("  ").SubstringContainsOnlyWhitespaceOrEmpty(0, 2));
  EXPECT_TRUE(StringView("\x20\x09\x0A\x0D")
                  .SubstringContainsOnlyWhitespaceOrEmpty(0, 4));
  EXPECT_FALSE(StringView(" a").SubstringContainsOnlyWhitespaceOrEmpty(0, 2));
  EXPECT_TRUE(StringView(" ").SubstringContainsOnlyWhitespaceOrEmpty(1, 1));
  EXPECT_TRUE(StringView("").SubstringContainsOnlyWhitespaceOrEmpty(0, 0));
  EXPECT_TRUE(
      StringView("  \nABC").SubstringContainsOnlyWhitespaceOrEmpty(0, 3));
  EXPECT_FALSE(StringView(" \u090A\n")
                   .SubstringContainsOnlyWhitespaceOrEmpty(
                       0, StringView(" \u090A\n").length()));
  EXPECT_FALSE(
      StringView("\n\x08\x1B").SubstringContainsOnlyWhitespaceOrEmpty(0, 3));
}

TEST(StringViewTest, ConstructionLiteral8) {
  // StringView(const LChar* chars);
  ASSERT_TRUE(StringView(kChars8).Is8Bit());
  EXPECT_FALSE(StringView(kChars8).IsNull());
  EXPECT_EQ(kChars8, StringView(kChars8).Characters8());
  EXPECT_EQ(5u, StringView(kChars8).length());
  EXPECT_EQ(kChars, StringView(kChars8));

  // StringView(const char* chars);
  ASSERT_TRUE(StringView(kChars).Is8Bit());
  EXPECT_FALSE(StringView(kChars).IsNull());
  EXPECT_EQ(kChars8, StringView(kChars).Characters8());
  EXPECT_EQ(5u, StringView(kChars).length());
  EXPECT_EQ(kChars, StringView(kChars));

  // StringView(const LChar* chars, unsigned length);
  ASSERT_TRUE(StringView(kChars8, 2u).Is8Bit());
  EXPECT_FALSE(StringView(kChars8, 2u).IsNull());
  EXPECT_EQ(2u, StringView(kChars8, 2u).length());
  EXPECT_EQ(StringView("12"), StringView(kChars8, 2u));
  EXPECT_EQ("12", StringView(kChars8, 2u));

  // StringView(const char* chars, unsigned length);
  ASSERT_TRUE(StringView(kChars, 2u).Is8Bit());
  EXPECT_FALSE(StringView(kChars, 2u).IsNull());
  EXPECT_EQ(2u, StringView(kChars, 2u).length());
  EXPECT_EQ(StringView("12"), StringView(kChars, 2u));
  EXPECT_EQ("12", StringView(kChars, 2u));
}

TEST(StringViewTest, ConstructionLiteral16) {
  // StringView(const UChar* chars);
  ASSERT_FALSE(StringView(kChars16).Is8Bit());
  EXPECT_FALSE(StringView(kChars16).IsNull());
  EXPECT_EQ(kChars16, StringView(kChars16).Characters16());
  EXPECT_EQ(5u, StringView(kChars16).length());
  EXPECT_EQ(String(kChars16), StringView(kChars16));

  // StringView(const UChar* chars, unsigned length);
  ASSERT_FALSE(StringView(kChars16, 2u).Is8Bit());
  EXPECT_FALSE(StringView(kChars16, 2u).IsNull());
  EXPECT_EQ(kChars16, StringView(kChars16, 2u).Characters16());
  EXPECT_EQ(StringView("12"), StringView(kChars16, 2u));
  EXPECT_EQ(StringView(reinterpret_cast<const UChar*>(u"12")),
            StringView(kChars16, 2u));
  EXPECT_EQ(2u, StringView(kChars16, 2u).length());
  EXPECT_EQ(String("12"), StringView(kChars16, 2u));
}

TEST(StringViewTest, ConstructionSpan8) {
  // StringView(base::span<const LChar> chars);
  const auto kCharsSpan8 = base::byte_span_from_cstring(kChars);
  ASSERT_TRUE(StringView(kCharsSpan8).Is8Bit());
  EXPECT_FALSE(StringView(kCharsSpan8).IsNull());
  EXPECT_EQ(kChars8, StringView(kCharsSpan8).Characters8());
  EXPECT_EQ(5u, StringView(kCharsSpan8).length());
  EXPECT_EQ(kChars, StringView(kCharsSpan8));
}

TEST(StringViewTest, ConstructionSpan16) {
  // StringView(base::span<const UChar> chars);
  const auto kCharsSpan16 = base::span_from_cstring(kCharsU);
  ASSERT_FALSE(StringView(kCharsSpan16).Is8Bit());
  EXPECT_FALSE(StringView(kCharsSpan16).IsNull());
  EXPECT_EQ(kChars16, StringView(kCharsSpan16).Characters16());
  EXPECT_EQ(5u, StringView(kCharsSpan16).length());
  EXPECT_EQ(String(kChars16), StringView(kCharsSpan16));
}

#if ENABLE_SECURITY_ASSERT
TEST(StringViewTest, OverflowInConstructor) {
  EXPECT_DEATH_IF_SUPPORTED(StringView(StringView("12"), 2, -1), "");
}

TEST(StringViewTest, OverflowInSet) {
  EXPECT_DEATH_IF_SUPPORTED(StringView(String("12"), 2, -1), "");
}
#endif  // ENABLE_SECURITY_ASSERT

TEST(StringViewTest, IsEmpty) {
  EXPECT_FALSE(StringView(kChars).empty());
  EXPECT_TRUE(StringView(kChars, 0).empty());
  EXPECT_FALSE(StringView(String(kChars)).empty());
  EXPECT_TRUE(StringView(String(kChars), 5).empty());
  EXPECT_TRUE(StringView(String(kChars), 4, 0).empty());
  EXPECT_TRUE(StringView().empty());
  EXPECT_TRUE(StringView("").empty());
  EXPECT_TRUE(StringView(reinterpret_cast<const UChar*>(u"")).empty());
  EXPECT_FALSE(StringView(kChars16).empty());
}

TEST(StringViewTest, ToString) {
  EXPECT_EQ(g_empty_string.Impl(), StringView("").ToString().Impl());
  EXPECT_EQ(g_null_atom.Impl(), StringView().ToString().Impl());
  // NOTE: All the construction tests also check toString().
}

TEST(StringViewTest, ToAtomicString) {
  EXPECT_EQ(g_null_atom.Impl(), StringView().ToAtomicString());
  EXPECT_EQ(g_empty_atom.Impl(), StringView("").ToAtomicString());
  EXPECT_EQ(AtomicString("12"), StringView(kChars8, 2u).ToAtomicString());
  // AtomicString will convert to 8bit if possible when creating the string.
  EXPECT_EQ(AtomicString("12").Impl(),
            StringView(kChars16, 2).ToAtomicString().Impl());
}

TEST(StringViewTest, ToStringImplSharing) {
  String string(kChars);
  EXPECT_EQ(string.Impl(), StringView(string).SharedImpl());
  EXPECT_EQ(string.Impl(), StringView(string).ToString().Impl());
  EXPECT_EQ(string.Impl(), StringView(string).ToAtomicString().Impl());
}

TEST(StringViewTest, NullString) {
  EXPECT_TRUE(StringView().IsNull());
  EXPECT_TRUE(StringView(String()).IsNull());
  EXPECT_TRUE(StringView(AtomicString()).IsNull());
  EXPECT_TRUE(StringView(static_cast<const char*>(nullptr)).IsNull());
  StringView view(kChars);
  EXPECT_FALSE(view.IsNull());
  view.Clear();
  EXPECT_TRUE(view.IsNull());
  EXPECT_EQ(String(), StringView());
  EXPECT_TRUE(StringView().ToString().IsNull());
  EXPECT_FALSE(EqualStringView(StringView(), ""));
  EXPECT_TRUE(EqualStringView(StringView(), StringView()));
  EXPECT_FALSE(EqualStringView(StringView(), "abc"));
  EXPECT_FALSE(EqualStringView("abc", StringView()));
  EXPECT_FALSE(EqualIgnoringASCIICase(StringView(), ""));
  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(), StringView()));
  EXPECT_FALSE(EqualIgnoringASCIICase(StringView(), "abc"));
  EXPECT_FALSE(EqualIgnoringASCIICase("abc", StringView()));
}

TEST(StringViewTest, IndexAccess) {
  StringView view8(kChars8);
  EXPECT_EQ('1', view8[0]);
  EXPECT_EQ('3', view8[2]);
  StringView view16(kChars16);
  EXPECT_EQ('1', view16[0]);
  EXPECT_EQ('3', view16[2]);
}

TEST(StringViewTest, EqualIgnoringASCIICase) {
  static const char* link8 = "link";
  static const char* link_caps8 = "LINK";
  static const char* non_ascii8 = "a\xE1";
  static const char* non_ascii_caps8 = "A\xE1";
  static const char* non_ascii_invalid8 = "a\xC1";

  static const UChar kLink16[5] = {0x006c, 0x0069, 0x006e, 0x006b, 0};  // link
  static const UChar kLinkCaps16[5] = {0x004c, 0x0049, 0x004e, 0x004b,
                                       0};                         // LINK
  static const UChar kNonASCII16[3] = {0x0061, 0x00e1, 0};         // a\xE1
  static const UChar kNonASCIICaps16[3] = {0x0041, 0x00e1, 0};     // A\xE1
  static const UChar kNonASCIIInvalid16[3] = {0x0061, 0x00c1, 0};  // a\xC1

  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(kLink16), link8));
  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(kLink16), kLinkCaps16));
  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(kLink16), link_caps8));
  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(link8), link_caps8));
  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(link8), kLink16));

  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(non_ascii8), non_ascii_caps8));
  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(non_ascii8), kNonASCIICaps16));
  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(kNonASCII16), kNonASCIICaps16));
  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(kNonASCII16), non_ascii_caps8));
  EXPECT_FALSE(
      EqualIgnoringASCIICase(StringView(non_ascii8), non_ascii_invalid8));
  EXPECT_FALSE(
      EqualIgnoringASCIICase(StringView(non_ascii8), kNonASCIIInvalid16));

  EXPECT_TRUE(EqualIgnoringASCIICase(StringView("link"), "lInK"));
  EXPECT_FALSE(EqualIgnoringASCIICase(StringView("link"), "INKL"));
  EXPECT_FALSE(
      EqualIgnoringASCIICase(StringView("link"), "link different length"));
  EXPECT_FALSE(
      EqualIgnoringASCIICase(StringView("link different length"), "link"));

  EXPECT_TRUE(EqualIgnoringASCIICase(StringView(""), ""));
}

TEST(StringViewTest, DeprecatedEqualIgnoringCase) {
  constexpr UChar kLongSAndKelvin[] = {0x017F, 0x212A, 0};
  EXPECT_TRUE(DeprecatedEqualIgnoringCase("SK", kLongSAndKelvin));
  EXPECT_TRUE(DeprecatedEqualIgnoringCase("sk", kLongSAndKelvin));

  // Turkish-specific mappings are not applied.
  constexpr UChar kSmallDotlessI[] = {0x0131, 0};
  constexpr UChar kCapitalDotI[] = {0x0130, 0};
  EXPECT_FALSE(DeprecatedEqualIgnoringCase("i", kSmallDotlessI));
  EXPECT_FALSE(DeprecatedEqualIgnoringCase("i", kCapitalDotI));

  // DeprecatedEqualIgnoringCase() has length-equality check.
  constexpr UChar kSmallSharpS[] = {0x00DF, 0};
  constexpr UChar kCapitalSharpS[] = {0x1E9E, 0};
  EXPECT_FALSE(DeprecatedEqualIgnoringCase("ss", kSmallSharpS));
  EXPECT_FALSE(DeprecatedEqualIgnoringCase("SS", kSmallSharpS));
  EXPECT_FALSE(DeprecatedEqualIgnoringCase("ss", kCapitalSharpS));
  EXPECT_FALSE(DeprecatedEqualIgnoringCase("SS", kCapitalSharpS));
  constexpr UChar kLigatureFFI[] = {0xFB03, 0};
  EXPECT_FALSE(DeprecatedEqualIgnoringCase("ffi", kLigatureFFI));

  constexpr UChar kLigatureFFIAndSSSS[] = {0xFB03, 's', 's', 's', 's', 0};
  constexpr UChar kFFIAndSharpSs[] = {'f', 'f', 'i', 0x00DF, 0x00DF, 0};
  EXPECT_TRUE(DeprecatedEqualIgnoringCase(kLigatureFFIAndSSSS, kFFIAndSharpSs));
}

TEST(StringViewTest, NextCodePointOffset) {
  StringView view8(kChars8);
  EXPECT_EQ(1u, view8.NextCodePointOffset(0));
  EXPECT_EQ(2u, view8.NextCodePointOffset(1));
  EXPECT_EQ(5u, view8.NextCodePointOffset(4));

  StringView view16(u"A\U0001F197X\U0001F232");
  ASSERT_EQ(6u, view16.length());
  EXPECT_EQ(1u, view16.NextCodePointOffset(0));
  EXPECT_EQ(3u, view16.NextCodePointOffset(1));
  EXPECT_EQ(3u, view16.NextCodePointOffset(2));
  EXPECT_EQ(4u, view16.NextCodePointOffset(3));
  EXPECT_EQ(6u, view16.NextCodePointOffset(4));

  const UChar kLead = 0xD800;
  StringView broken1(&kLead, 1);
  EXPECT_EQ(1u, broken1.NextCodePointOffset(0));

  const UChar kLeadAndNotTrail[] = {0xD800, 0x20, 0};
  StringView broken2(kLeadAndNotTrail);
  EXPECT_EQ(1u, broken2.NextCodePointOffset(0));
  EXPECT_EQ(2u, broken2.NextCodePointOffset(1));

  const UChar kTrail = 0xDC00;
  StringView broken3(&kTrail, 1);
  EXPECT_EQ(1u, broken3.NextCodePointOffset(0));
}

}  // namespace WTF
