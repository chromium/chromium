// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

const LChar* Address8(const StringImpl& impl, size_t offset = 0) {
  return offset ? impl.Span8().subspan(offset).data() : impl.Span8().data();
}

const LChar* Address8(const String& str, size_t offset = 0) {
  return offset ? str.Span8().subspan(offset).data() : str.Span8().data();
}

const LChar* Address8(StringView view, size_t offset = 0) {
  return offset ? view.Span8().subspan(offset).data() : view.Span8().data();
}

const UChar* Address16(const StringImpl& impl, size_t offset = 0) {
  return offset ? impl.Span16().subspan(offset).data() : impl.Span16().data();
}

const UChar* Address16(const String& str, size_t offset = 0) {
  return offset ? str.Span16().subspan(offset).data() : str.Span16().data();
}

const UChar* Address16(const AtomicString& str, size_t offset = 0) {
  return offset ? str.Span16().subspan(offset).data() : str.Span16().data();
}

const UChar* Address16(StringView view, size_t offset = 0) {
  return offset ? view.Span16().subspan(offset).data() : view.Span16().data();
}

const char kChars[] = "12345";
const char16_t kCharsU[] = u"12345";
const LChar* const kChars8 = reinterpret_cast<const LChar*>(kChars);
const UChar* const kChars16 = reinterpret_cast<const UChar*>(kCharsU);
const base::span<const LChar> kSpan8 = base::byte_span_from_cstring(kChars);
const base::span<const UChar> kSpan16 = base::span_from_cstring(kCharsU);

}  // namespace

TEST(StringViewTest, ConstructionStringImpl8) {
  scoped_refptr<StringImpl> impl8_bit = StringImpl::Create(kSpan8);

  // StringView(StringImpl*);
  ASSERT_TRUE(StringView(impl8_bit.get()).Is8Bit());
  EXPECT_FALSE(StringView(impl8_bit.get()).IsNull());
  EXPECT_EQ(Address8(*impl8_bit), Address8(StringView(impl8_bit.get())));
  EXPECT_EQ(impl8_bit->length(), StringView(impl8_bit.get()).length());
  EXPECT_EQ(kChars, StringView(impl8_bit.get()));

  // StringView(StringImpl*, unsigned offset);
  ASSERT_TRUE(StringView(impl8_bit.get(), 2).Is8Bit());
  EXPECT_FALSE(StringView(impl8_bit.get(), 2).IsNull());
  EXPECT_EQ(Address8(*impl8_bit, 2u), Address8(StringView(impl8_bit.get(), 2)));
  EXPECT_EQ(3u, StringView(impl8_bit.get(), 2).length());
  EXPECT_EQ(StringView("345"), StringView(impl8_bit.get(), 2));
  EXPECT_EQ("345", StringView(impl8_bit.get(), 2));

  // StringView(StringImpl*, unsigned offset, unsigned length);
  ASSERT_TRUE(StringView(impl8_bit.get(), 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(impl8_bit.get(), 2, 1).IsNull());
  EXPECT_EQ(Address8(*impl8_bit, 2u),
            Address8(StringView(impl8_bit.get(), 2, 1)));
  EXPECT_EQ(1u, StringView(impl8_bit.get(), 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(impl8_bit.get(), 2, 1));
  EXPECT_EQ("3", StringView(impl8_bit.get(), 2, 1));
}

TEST(StringViewTest, ConstructionStringImpl16) {
  scoped_refptr<StringImpl> impl16_bit = StringImpl::Create(kSpan16);

  // StringView(StringImpl*);
  ASSERT_FALSE(StringView(impl16_bit.get()).Is8Bit());
  EXPECT_FALSE(StringView(impl16_bit.get()).IsNull());
  EXPECT_EQ(Address16(*impl16_bit), Address16(StringView(impl16_bit.get())));
  EXPECT_EQ(impl16_bit->length(), StringView(impl16_bit.get()).length());
  EXPECT_EQ(kChars, StringView(impl16_bit.get()));

  // StringView(StringImpl*, unsigned offset);
  ASSERT_FALSE(StringView(impl16_bit.get(), 2).Is8Bit());
  EXPECT_FALSE(StringView(impl16_bit.get(), 2).IsNull());
  EXPECT_EQ(Address16(*impl16_bit, 2u),
            Address16(StringView(impl16_bit.get(), 2)));
  EXPECT_EQ(3u, StringView(impl16_bit.get(), 2).length());
  EXPECT_EQ(StringView("345"), StringView(impl16_bit.get(), 2));
  EXPECT_EQ("345", StringView(impl16_bit.get(), 2));

  // StringView(StringImpl*, unsigned offset, unsigned length);
  ASSERT_FALSE(StringView(impl16_bit.get(), 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(impl16_bit.get(), 2, 1).IsNull());
  EXPECT_EQ(Address16(*impl16_bit, 2u),
            Address16(StringView(impl16_bit.get(), 2, 1)));
  EXPECT_EQ(1u, StringView(impl16_bit.get(), 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(impl16_bit.get(), 2, 1));
  EXPECT_EQ("3", StringView(impl16_bit.get(), 2, 1));
}

TEST(StringViewTest, ConstructionStringImplRef8) {
  scoped_refptr<StringImpl> impl8_bit = StringImpl::Create(kSpan8);

  // StringView(StringImpl&);
  ASSERT_TRUE(StringView(*impl8_bit).Is8Bit());
  EXPECT_FALSE(StringView(*impl8_bit).IsNull());
  EXPECT_EQ(Address8(*impl8_bit), Address8(StringView(*impl8_bit)));
  EXPECT_EQ(impl8_bit->length(), StringView(*impl8_bit).length());
  EXPECT_EQ(kChars, StringView(*impl8_bit));

  // StringView(StringImpl&, unsigned offset);
  ASSERT_TRUE(StringView(*impl8_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(*impl8_bit, 2).IsNull());
  EXPECT_EQ(Address8(*impl8_bit, 2u), Address8(StringView(*impl8_bit, 2)));
  EXPECT_EQ(3u, StringView(*impl8_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(*impl8_bit, 2));
  EXPECT_EQ("345", StringView(*impl8_bit, 2));

  // StringView(StringImpl&, unsigned offset, unsigned length);
  ASSERT_TRUE(StringView(*impl8_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(*impl8_bit, 2, 1).IsNull());
  EXPECT_EQ(Address8(*impl8_bit, 2u), Address8(StringView(*impl8_bit, 2, 1)));
  EXPECT_EQ(1u, StringView(*impl8_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(*impl8_bit, 2, 1));
  EXPECT_EQ("3", StringView(*impl8_bit, 2, 1));
}

TEST(StringViewTest, ConstructionStringImplRef16) {
  scoped_refptr<StringImpl> impl16_bit = StringImpl::Create(kSpan16);

  // StringView(StringImpl&);
  ASSERT_FALSE(StringView(*impl16_bit).Is8Bit());
  EXPECT_FALSE(StringView(*impl16_bit).IsNull());
  EXPECT_EQ(Address16(*impl16_bit), Address16(StringView(*impl16_bit)));
  EXPECT_EQ(impl16_bit->length(), StringView(*impl16_bit).length());
  EXPECT_EQ(kChars, StringView(*impl16_bit));

  // StringView(StringImpl&, unsigned offset);
  ASSERT_FALSE(StringView(*impl16_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(*impl16_bit, 2).IsNull());
  EXPECT_EQ(Address16(*impl16_bit, 2u), Address16(StringView(*impl16_bit, 2)));
  EXPECT_EQ(3u, StringView(*impl16_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(*impl16_bit, 2));
  EXPECT_EQ("345", StringView(*impl16_bit, 2));

  // StringView(StringImpl&, unsigned offset, unsigned length);
  ASSERT_FALSE(StringView(*impl16_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(*impl16_bit, 2, 1).IsNull());
  EXPECT_EQ(Address16(*impl16_bit, 2u),
            Address16(StringView(*impl16_bit, 2, 1)));
  EXPECT_EQ(1u, StringView(*impl16_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(*impl16_bit, 2, 1));
  EXPECT_EQ("3", StringView(*impl16_bit, 2, 1));
}

TEST(StringViewTest, ConstructionString8) {
  String string8_bit = String(StringImpl::Create(kSpan8));

  // StringView(const String&);
  ASSERT_TRUE(StringView(string8_bit).Is8Bit());
  EXPECT_FALSE(StringView(string8_bit).IsNull());
  EXPECT_EQ(Address8(string8_bit), Address8(StringView(string8_bit)));
  EXPECT_EQ(string8_bit.length(), StringView(string8_bit).length());
  EXPECT_EQ(kChars, StringView(string8_bit));

  // StringView(const String&, unsigned offset);
  ASSERT_TRUE(StringView(string8_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(string8_bit, 2).IsNull());
  EXPECT_EQ(Address8(string8_bit, 2u), Address8(StringView(string8_bit, 2)));
  EXPECT_EQ(3u, StringView(string8_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(string8_bit, 2));
  EXPECT_EQ("345", StringView(string8_bit, 2));

  // StringView(const String&, unsigned offset, unsigned length);
  ASSERT_TRUE(StringView(string8_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(string8_bit, 2, 1).IsNull());
  EXPECT_EQ(Address8(string8_bit, 2u), Address8(StringView(string8_bit, 2, 1)));
  EXPECT_EQ(1u, StringView(string8_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(string8_bit, 2, 1));
  EXPECT_EQ("3", StringView(string8_bit, 2, 1));
}

TEST(StringViewTest, ConstructionString16) {
  String string16_bit = String(StringImpl::Create(kSpan16));

  // StringView(const String&);
  ASSERT_FALSE(StringView(string16_bit).Is8Bit());
  EXPECT_FALSE(StringView(string16_bit).IsNull());
  EXPECT_EQ(Address16(string16_bit), Address16(StringView(string16_bit)));
  EXPECT_EQ(string16_bit.length(), StringView(string16_bit).length());
  EXPECT_EQ(kChars, StringView(string16_bit));

  // StringView(const String&, unsigned offset);
  ASSERT_FALSE(StringView(string16_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(string16_bit, 2).IsNull());
  EXPECT_EQ(Address16(string16_bit, 2u),
            Address16(StringView(string16_bit, 2)));
  EXPECT_EQ(3u, StringView(string16_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(string16_bit, 2));
  EXPECT_EQ("345", StringView(string16_bit, 2));

  // StringView(const String&, unsigned offset, unsigned length);
  ASSERT_FALSE(StringView(string16_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(string16_bit, 2, 1).IsNull());
  EXPECT_EQ(Address16(string16_bit, 2u),
            Address16(StringView(string16_bit, 2, 1)));
  EXPECT_EQ(1u, StringView(string16_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(string16_bit, 2, 1));
  EXPECT_EQ("3", StringView(string16_bit, 2, 1));
}

TEST(StringViewTest, ConstructionAtomicString8) {
  AtomicString atom8_bit = AtomicString(StringImpl::Create(kSpan8));

  // StringView(const AtomicString&);
  ASSERT_TRUE(StringView(atom8_bit).Is8Bit());
  EXPECT_FALSE(StringView(atom8_bit).IsNull());
  EXPECT_EQ(atom8_bit.Span8().data(), StringView(atom8_bit).Span8().data());
  EXPECT_EQ(atom8_bit.length(), StringView(atom8_bit).length());
  EXPECT_EQ(kChars, StringView(atom8_bit));

  // StringView(const AtomicString&, unsigned offset);
  ASSERT_TRUE(StringView(atom8_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(atom8_bit, 2).IsNull());
  EXPECT_EQ(atom8_bit.Span8().subspan(2u).data(),
            StringView(atom8_bit, 2).Span8().data());
  EXPECT_EQ(3u, StringView(atom8_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(atom8_bit, 2));
  EXPECT_EQ("345", StringView(atom8_bit, 2));

  // StringView(const AtomicString&, unsigned offset, unsigned length);
  ASSERT_TRUE(StringView(atom8_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(atom8_bit, 2, 1).IsNull());
  EXPECT_EQ(atom8_bit.Span8().subspan(2u).data(),
            StringView(atom8_bit, 2, 1).Span8().data());
  EXPECT_EQ(1u, StringView(atom8_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(atom8_bit, 2, 1));
  EXPECT_EQ("3", StringView(atom8_bit, 2, 1));
}

TEST(StringViewTest, ConstructionAtomicString16) {
  AtomicString atom16_bit = AtomicString(StringImpl::Create(kSpan16));

  // StringView(const AtomicString&);
  ASSERT_FALSE(StringView(atom16_bit).Is8Bit());
  EXPECT_FALSE(StringView(atom16_bit).IsNull());
  EXPECT_EQ(Address16(atom16_bit), Address16(StringView(atom16_bit)));
  EXPECT_EQ(atom16_bit.length(), StringView(atom16_bit).length());
  EXPECT_EQ(kChars, StringView(atom16_bit));

  // StringView(const AtomicString&, unsigned offset);
  ASSERT_FALSE(StringView(atom16_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(atom16_bit, 2).IsNull());
  EXPECT_EQ(Address16(atom16_bit, 2u), Address16(StringView(atom16_bit, 2)));
  EXPECT_EQ(3u, StringView(atom16_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(atom16_bit, 2));
  EXPECT_EQ("345", StringView(atom16_bit, 2));

  // StringView(const AtomicString&, unsigned offset, unsigned length);
  ASSERT_FALSE(StringView(atom16_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(atom16_bit, 2, 1).IsNull());
  EXPECT_EQ(Address16(atom16_bit, 2u), Address16(StringView(atom16_bit, 2, 1)));
  EXPECT_EQ(1u, StringView(atom16_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(atom16_bit, 2, 1));
  EXPECT_EQ("3", StringView(atom16_bit, 2, 1));
}

TEST(StringViewTest, ConstructionStringView8) {
  StringView view8_bit = StringView(base::byte_span_from_cstring(kChars));

  // StringView(StringView&);
  ASSERT_TRUE(StringView(view8_bit).Is8Bit());
  EXPECT_FALSE(StringView(view8_bit).IsNull());
  EXPECT_EQ(Address8(view8_bit), Address8(StringView(view8_bit)));
  EXPECT_EQ(view8_bit.length(), StringView(view8_bit).length());
  EXPECT_EQ(kChars, StringView(view8_bit));

  // StringView(const StringView&, unsigned offset);
  ASSERT_TRUE(StringView(view8_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(view8_bit, 2).IsNull());
  EXPECT_EQ(Address8(view8_bit, 2u), Address8(StringView(view8_bit, 2)));
  EXPECT_EQ(3u, StringView(view8_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(view8_bit, 2));
  EXPECT_EQ("345", StringView(view8_bit, 2));

  // StringView(const StringView&, unsigned offset, unsigned length);
  ASSERT_TRUE(StringView(view8_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(view8_bit, 2, 1).IsNull());
  EXPECT_EQ(Address8(view8_bit, 2u), Address8(StringView(view8_bit, 2, 1)));
  EXPECT_EQ(1u, StringView(view8_bit, 2, 1).length());
  EXPECT_EQ(StringView("3"), StringView(view8_bit, 2, 1));
  EXPECT_EQ("3", StringView(view8_bit, 2, 1));
}

TEST(StringViewTest, ConstructionStringView16) {
  StringView view16_bit = StringView(base::span_from_cstring(kCharsU));

  // StringView(StringView&);
  ASSERT_FALSE(StringView(view16_bit).Is8Bit());
  EXPECT_FALSE(StringView(view16_bit).IsNull());
  EXPECT_EQ(Address16(view16_bit), Address16(StringView(view16_bit)));
  EXPECT_EQ(view16_bit.length(), StringView(view16_bit).length());
  EXPECT_EQ(kChars, StringView(view16_bit));

  // StringView(const StringView&, unsigned offset);
  ASSERT_FALSE(StringView(view16_bit, 2).Is8Bit());
  EXPECT_FALSE(StringView(view16_bit, 2).IsNull());
  EXPECT_EQ(Address16(view16_bit, 2u), Address16(StringView(view16_bit, 2)));
  EXPECT_EQ(3u, StringView(view16_bit, 2).length());
  EXPECT_EQ(StringView("345"), StringView(view16_bit, 2));
  EXPECT_EQ("345", StringView(view16_bit, 2));

  // StringView(const StringView&, unsigned offset, unsigned length);
  ASSERT_FALSE(StringView(view16_bit, 2, 1).Is8Bit());
  EXPECT_FALSE(StringView(view16_bit, 2, 1).IsNull());
  EXPECT_EQ(Address16(view16_bit, 2u), Address16(StringView(view16_bit, 2, 1)));
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
  // StringView(const char* chars);
  ASSERT_TRUE(StringView(kChars).Is8Bit());
  EXPECT_FALSE(StringView(kChars).IsNull());
  EXPECT_EQ(kChars8, Address8(StringView(kChars)));
  EXPECT_EQ(5u, StringView(kChars).length());
  EXPECT_EQ(kChars, StringView(kChars));

  // StringView(base::span<const LChar> chars);
  StringView view2(base::as_byte_span(kChars).first(2u));
  ASSERT_TRUE(view2.Is8Bit());
  EXPECT_FALSE(view2.IsNull());
  EXPECT_EQ(2u, view2.length());
  EXPECT_EQ(StringView("12"), view2);
  EXPECT_EQ("12", view2);
}

TEST(StringViewTest, ConstructionLiteral16) {
  // StringView(const UChar* chars);
  ASSERT_FALSE(StringView(kChars16).Is8Bit());
  EXPECT_FALSE(StringView(kChars16).IsNull());
  EXPECT_EQ(kChars16, Address16(StringView(kChars16)));
  EXPECT_EQ(5u, StringView(kChars16).length());
  EXPECT_EQ(String(kChars16), StringView(kChars16));

  // StringView(base::span<const UChar> chars);
  StringView uchar2(base::span_from_cstring(kCharsU).first(2u));
  ASSERT_FALSE(uchar2.Is8Bit());
  EXPECT_FALSE(uchar2.IsNull());
  EXPECT_EQ(kChars16, Address16(uchar2));
  EXPECT_EQ(StringView("12"), uchar2);
  EXPECT_EQ(StringView(reinterpret_cast<const UChar*>(u"12")), uchar2);
  EXPECT_EQ(2u, uchar2.length());
  EXPECT_EQ(String("12"), uchar2);
}

TEST(StringViewTest, ConstructionSpan8) {
  // StringView(base::span<const LChar> chars);
  const auto kCharsSpan8 = base::byte_span_from_cstring(kChars);
  ASSERT_TRUE(StringView(kCharsSpan8).Is8Bit());
  EXPECT_FALSE(StringView(kCharsSpan8).IsNull());
  EXPECT_EQ(kChars8, Address8(StringView(kCharsSpan8)));
  EXPECT_EQ(5u, StringView(kCharsSpan8).length());
  EXPECT_EQ(kChars, StringView(kCharsSpan8));
}

TEST(StringViewTest, ConstructionSpan16) {
  // StringView(base::span<const UChar> chars);
  const auto kCharsSpan16 = base::span_from_cstring(kCharsU);
  ASSERT_FALSE(StringView(kCharsSpan16).Is8Bit());
  EXPECT_FALSE(StringView(kCharsSpan16).IsNull());
  EXPECT_EQ(kChars16, Address16(StringView(kCharsSpan16)));
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
  EXPECT_TRUE(StringView(base::as_byte_span(kChars).first(0u)).empty());
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
  EXPECT_EQ(AtomicString("12"),
            StringView(base::as_byte_span(kChars).first(2u)).ToAtomicString());
  // AtomicString will convert to 8bit if possible when creating the string.
  EXPECT_EQ(AtomicString("12").Impl(),
            StringView(base::span_from_cstring(kCharsU).first(2u))
                .ToAtomicString()
                .Impl());
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
  EXPECT_FALSE(EqualIgnoringAsciiCase(StringView(), ""));
  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(), StringView()));
  EXPECT_FALSE(EqualIgnoringAsciiCase(StringView(), "abc"));
  EXPECT_FALSE(EqualIgnoringAsciiCase("abc", StringView()));
}

TEST(StringViewTest, IndexAccess) {
  StringView view8(kChars);
  EXPECT_EQ('1', view8[0]);
  EXPECT_EQ('3', view8[2]);
  StringView view16(kChars16);
  EXPECT_EQ('1', view16[0]);
  EXPECT_EQ('3', view16[2]);
}

TEST(StringViewTest, EqualIgnoringAsciiCase) {
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

  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(kLink16), link8));
  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(kLink16), kLinkCaps16));
  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(kLink16), link_caps8));
  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(link8), link_caps8));
  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(link8), kLink16));

  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(non_ascii8), non_ascii_caps8));
  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(non_ascii8), kNonASCIICaps16));
  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(kNonASCII16), kNonASCIICaps16));
  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(kNonASCII16), non_ascii_caps8));
  EXPECT_FALSE(
      EqualIgnoringAsciiCase(StringView(non_ascii8), non_ascii_invalid8));
  EXPECT_FALSE(
      EqualIgnoringAsciiCase(StringView(non_ascii8), kNonASCIIInvalid16));

  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView("link"), "lInK"));
  EXPECT_FALSE(EqualIgnoringAsciiCase(StringView("link"), "INKL"));
  EXPECT_FALSE(
      EqualIgnoringAsciiCase(StringView("link"), "link different length"));
  EXPECT_FALSE(
      EqualIgnoringAsciiCase(StringView("link different length"), "link"));

  EXPECT_TRUE(EqualIgnoringAsciiCase(StringView(""), ""));
}

TEST(StringViewTest, CodeUnitCompareIgnoringAsciiCase) {
  StringView a8("abc");
  StringView b8("abc");
  StringView c8("ABC");
  StringView d8("abd");
  StringView e8("");

  EXPECT_EQ(0, blink::CodeUnitCompareIgnoringAsciiCase(a8, b8));
  EXPECT_EQ(0, blink::CodeUnitCompareIgnoringAsciiCase(a8, c8));
  EXPECT_LT(blink::CodeUnitCompareIgnoringAsciiCase(a8, d8), 0);
  EXPECT_GT(blink::CodeUnitCompareIgnoringAsciiCase(d8, a8), 0);
  EXPECT_GT(blink::CodeUnitCompareIgnoringAsciiCase(a8, e8), 0);
  EXPECT_LT(blink::CodeUnitCompareIgnoringAsciiCase(e8, a8), 0);

  StringView a16(u"abc");
  StringView b16(u"abc");
  StringView c16(u"ABC");
  StringView d16(u"abd");
  StringView e16(u"");

  EXPECT_EQ(0, blink::CodeUnitCompareIgnoringAsciiCase(a16, b16));
  EXPECT_EQ(0, blink::CodeUnitCompareIgnoringAsciiCase(a16, c16));
  EXPECT_LT(blink::CodeUnitCompareIgnoringAsciiCase(a16, d16), 0);
  EXPECT_GT(blink::CodeUnitCompareIgnoringAsciiCase(d16, a16), 0);
  EXPECT_GT(blink::CodeUnitCompareIgnoringAsciiCase(a16, e16), 0);
  EXPECT_LT(blink::CodeUnitCompareIgnoringAsciiCase(e16, a16), 0);

  EXPECT_EQ(0, blink::CodeUnitCompareIgnoringAsciiCase(a8, a16));
  EXPECT_EQ(0, blink::CodeUnitCompareIgnoringAsciiCase(a8, c16));
  EXPECT_LT(blink::CodeUnitCompareIgnoringAsciiCase(a8, d16), 0);
  EXPECT_GT(blink::CodeUnitCompareIgnoringAsciiCase(d16, a8), 0);
  EXPECT_GT(blink::CodeUnitCompareIgnoringAsciiCase(a8, e16), 0);
  EXPECT_LT(blink::CodeUnitCompareIgnoringAsciiCase(e16, a8), 0);

  EXPECT_FALSE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(a8, b8));
  EXPECT_FALSE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(a8, c8));
  EXPECT_TRUE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(a8, d8));
  EXPECT_FALSE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(d8, a8));
  EXPECT_FALSE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(a8, e8));
  EXPECT_TRUE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(e8, a8));

  EXPECT_FALSE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(a16, b16));
  EXPECT_FALSE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(a16, c16));
  EXPECT_TRUE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(a16, d16));
  EXPECT_FALSE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(d16, a16));
  EXPECT_FALSE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(a16, e16));
  EXPECT_TRUE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(e16, a16));

  StringView short8("ab");
  StringView long8("abc");
  EXPECT_LT(blink::CodeUnitCompareIgnoringAsciiCase(short8, long8), 0);
  EXPECT_GT(blink::CodeUnitCompareIgnoringAsciiCase(long8, short8), 0);
  EXPECT_TRUE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(short8, long8));
  EXPECT_FALSE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(long8, short8));

  StringView short16(u"ab");
  StringView long16(u"abc");
  EXPECT_LT(blink::CodeUnitCompareIgnoringAsciiCase(short16, long16), 0);
  EXPECT_GT(blink::CodeUnitCompareIgnoringAsciiCase(long16, short16), 0);
  EXPECT_TRUE(blink::CodeUnitCompareIgnoringAsciiCaseLessThan(short16, long16));
  EXPECT_FALSE(
      blink::CodeUnitCompareIgnoringAsciiCaseLessThan(long16, short16));

  StringView non_ascii8("ab\xE1");
  StringView non_ascii16(u"ab\u00E1");
  EXPECT_EQ(0,
            blink::CodeUnitCompareIgnoringAsciiCase(non_ascii8, non_ascii16));
  EXPECT_FALSE(
      blink::CodeUnitCompareIgnoringAsciiCaseLessThan(non_ascii8, non_ascii16));
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
  EXPECT_TRUE(
      DeprecatedEqualIgnoringCase(base::span_from_cstring(kLigatureFFIAndSSSS),
                                  base::span_from_cstring(kFFIAndSharpSs)));
}

TEST(StringViewTest, NextCodePointOffset) {
  StringView view8(kChars);
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
  StringView broken1(base::span_from_ref(kLead));
  EXPECT_EQ(1u, broken1.NextCodePointOffset(0));

  const UChar kLeadAndNotTrail[] = {0xD800, 0x20, 0};
  StringView broken2(kLeadAndNotTrail);
  EXPECT_EQ(1u, broken2.NextCodePointOffset(0));
  EXPECT_EQ(2u, broken2.NextCodePointOffset(1));

  const UChar kTrail = 0xDC00;
  StringView broken3(base::span_from_ref(kTrail));
  EXPECT_EQ(1u, broken3.NextCodePointOffset(0));
}

TEST(StringViewTest, FindSubstring) {
  EXPECT_EQ(0u, StringView().find(StringView()));
  EXPECT_EQ(0u, StringView("").find(StringView()));
  EXPECT_EQ(0u, StringView(u"").find(StringView()));
  EXPECT_EQ(kNotFound, StringView().find("a"));
  EXPECT_EQ(kNotFound, StringView("").find("a"));
  EXPECT_EQ(kNotFound, StringView(u"").find("a"));

  StringView view8("abcdeabcde");
  ASSERT_TRUE(view8.Is8Bit());
  EXPECT_EQ(0u, view8.find(""));
  EXPECT_EQ(4u, view8.find("", 4));
  EXPECT_EQ(view8.length(), view8.find("", view8.length()));
  EXPECT_EQ(kNotFound, view8.find("", view8.length() + 1));

  EXPECT_EQ(0u, view8.find("ab"));
  EXPECT_EQ(5u, view8.find("ab", 1));
  EXPECT_EQ(5u, view8.find("ab", 5));
  EXPECT_EQ(kNotFound, view8.find("ab", 6));
  EXPECT_EQ(kNotFound, view8.find("ab", view8.length() - 1));
  EXPECT_EQ(kNotFound, view8.find("ab", view8.length()));
  EXPECT_EQ(kNotFound, view8.find("ab", view8.length() + 1));
  EXPECT_EQ(0u, view8.find(view8));
  EXPECT_EQ(kNotFound, view8.find(view8, 1));
  EXPECT_EQ(kNotFound, view8.find("abcdeabcdea"));

  EXPECT_EQ(0u, view8.find(u"ab"));
  EXPECT_EQ(5u, view8.find(u"ab", 1));
  EXPECT_EQ(5u, view8.find(u"ab", 5));
  EXPECT_EQ(kNotFound, view8.find(u"ab", 6));
  EXPECT_EQ(kNotFound, view8.find(u"ab", view8.length() - 1));
  EXPECT_EQ(kNotFound, view8.find(u"ab", view8.length()));
  EXPECT_EQ(kNotFound, view8.find(u"ab", view8.length() + 1));
  EXPECT_EQ(0u, view8.find(u"abcdeabcde"));
  EXPECT_EQ(kNotFound, view8.find(u"abcdeabcde", 1));
  EXPECT_EQ(kNotFound, view8.find(u"abcdeabcdea"));

  StringView view8_with_null(base::byte_span_from_cstring("as\0cii"));
  ASSERT_TRUE(view8_with_null.Is8Bit());
  EXPECT_EQ(kNotFound, view8_with_null.find("ascii"));
  const StringView kSNulC(base::byte_span_from_cstring("s\0c"));
  EXPECT_EQ(1u, view8_with_null.find(kSNulC));
  EXPECT_EQ(1u, view8_with_null.find(kSNulC, 1));
  EXPECT_EQ(3u, view8_with_null.find("c"));
  const StringView kNul(base::byte_span_from_cstring("\0"));
  EXPECT_EQ(2u, view8_with_null.find(kNul));
  EXPECT_EQ(kNotFound, view8_with_null.find(kNul, 3));

  StringView view16(u"abcde\u1234abcde");
  ASSERT_FALSE(view16.Is8Bit());
  EXPECT_EQ(0u, view16.find("ab"));
  EXPECT_EQ(2u, view16.find("cd"));
  EXPECT_EQ(6u, view16.find("ab", 5));
  EXPECT_EQ(kNotFound, view16.find("ab", 7));
  EXPECT_EQ(5u, view16.find(u"\u1234a"));
  EXPECT_EQ(kNotFound, view16.find("abd"));
  EXPECT_EQ(kNotFound, view16.find(u"\u1234a", 6));

  StringView view16_with_null(base::span_from_cstring(u"asci\0i"));
  ASSERT_FALSE(view16_with_null.Is8Bit());
  const StringView kNul16(base::span_from_cstring(u"\0"));
  EXPECT_EQ(4u, view16_with_null.find(kNul));
  EXPECT_EQ(4u, view16_with_null.find(kNul16));
  EXPECT_EQ(4u, view16_with_null.find(kNul16, 4));
  EXPECT_EQ(5u, view16_with_null.find("i", 4));
  EXPECT_EQ(kNotFound, view16_with_null.find(kNul16, 5));
}

TEST(StringViewTest, FindChar) {
  EXPECT_EQ(kNotFound, StringView().find(0));
  EXPECT_EQ(kNotFound, StringView("").find(0));
  EXPECT_EQ(kNotFound, StringView(u"").find(0));
  EXPECT_EQ(kNotFound, StringView().find('a'));
  EXPECT_EQ(kNotFound, StringView("").find('a'));
  EXPECT_EQ(kNotFound, StringView(u"").find('a'));

  StringView view8("abcdeabcde");
  ASSERT_TRUE(view8.Is8Bit());
  EXPECT_EQ(0u, view8.find('a'));
  EXPECT_EQ(2u, view8.find('c'));
  EXPECT_EQ(4u, view8.find('e'));
  EXPECT_EQ(kNotFound, view8.find('z'));
  EXPECT_EQ(0u, view8.find('a', 0));
  EXPECT_EQ(5u, view8.find('a', 1));
  EXPECT_EQ(5u, view8.find('a', 5));
  EXPECT_EQ(kNotFound, view8.find('a', 6));
  EXPECT_EQ(9u, view8.find('e', 5));
  EXPECT_EQ(kNotFound, view8.find('e', 10));
  EXPECT_EQ(kNotFound, view8.find(UChar(0x1234)));

  StringView view8_with_null(base::byte_span_from_cstring("as\0cii"));
  ASSERT_TRUE(view8_with_null.Is8Bit());
  EXPECT_EQ(2u, view8_with_null.find('\0'));
  EXPECT_EQ(2u, view8_with_null.find('\0', 2));
  EXPECT_EQ(3u, view8_with_null.find('c'));
  EXPECT_EQ(kNotFound, view8_with_null.find('\0', 3));

  StringView view16(u"abcde\u1234abcde");
  ASSERT_FALSE(view16.Is8Bit());
  EXPECT_EQ(0u, view16.find('a'));
  EXPECT_EQ(2u, view16.find('c'));
  EXPECT_EQ(5u, view16.find(UChar(0x1234)));
  EXPECT_EQ(kNotFound, view16.find('z'));
  EXPECT_EQ(6u, view16.find('a', 1));
  EXPECT_EQ(kNotFound, view16.find(UChar(0x1234), 6));

  StringView view16_with_null(base::span_from_cstring(u"asci\0i"));
  ASSERT_FALSE(view16_with_null.Is8Bit());
  EXPECT_EQ(4u, view16_with_null.find(UChar(0)));
  EXPECT_EQ(4u, view16_with_null.find(UChar(0), 4));
  EXPECT_EQ(5u, view16_with_null.find('i', 4));
  EXPECT_EQ(kNotFound, view16_with_null.find(UChar(0), 5));
}

TEST(StringViewTest, RfindChar) {
  EXPECT_EQ(kNotFound, StringView().rfind(0));
  EXPECT_EQ(kNotFound, StringView("").rfind(0));
  EXPECT_EQ(kNotFound, StringView(u"").rfind(0));
  EXPECT_EQ(kNotFound, StringView().rfind('a'));
  EXPECT_EQ(kNotFound, StringView("").rfind('a'));
  EXPECT_EQ(kNotFound, StringView(u"").rfind('a'));

  StringView view8("abcdeabcde");
  ASSERT_TRUE(view8.Is8Bit());
  EXPECT_EQ(5u, view8.rfind('a'));
  EXPECT_EQ(7u, view8.rfind('c'));
  EXPECT_EQ(9u, view8.rfind('e'));
  EXPECT_EQ(kNotFound, view8.rfind('z'));
  EXPECT_EQ(5u, view8.rfind('a', 5));
  EXPECT_EQ(5u, view8.rfind('a', 9));
  EXPECT_EQ(0u, view8.rfind('a', 4));
  EXPECT_EQ(kNotFound, view8.rfind('e', 3));
  EXPECT_EQ(0u, view8.rfind('a', 4));
  EXPECT_EQ(kNotFound, view8.rfind('e', 0));
  EXPECT_EQ(kNotFound, view8.rfind(UChar(0x1234)));

  StringView view8_with_null(base::byte_span_from_cstring("as\0cii"));
  ASSERT_TRUE(view8_with_null.Is8Bit());
  EXPECT_EQ(2u, view8_with_null.rfind('\0'));
  EXPECT_EQ(2u, view8_with_null.rfind('\0', 2));
  EXPECT_EQ(1u, view8_with_null.rfind('s'));
  EXPECT_EQ(kNotFound, view8_with_null.rfind('\0', 1));

  StringView view16(u"abcde\u1234abcde");
  ASSERT_FALSE(view16.Is8Bit());
  EXPECT_EQ(6u, view16.rfind('a'));
  EXPECT_EQ(8u, view16.rfind('c'));
  EXPECT_EQ(5u, view16.rfind(UChar(0x1234)));
  EXPECT_EQ(kNotFound, view16.rfind('z'));
  EXPECT_EQ(0u, view16.rfind('a', 5));
  EXPECT_EQ(kNotFound, view16.rfind(UChar(0x1234), 4));

  StringView view16_with_null(base::span_from_cstring(u"asci\0i"));
  ASSERT_FALSE(view16_with_null.Is8Bit());
  EXPECT_EQ(4u, view16_with_null.rfind(UChar(0)));
  EXPECT_EQ(4u, view16_with_null.rfind(UChar(0), 4));
  EXPECT_EQ(3u, view16_with_null.rfind('i', 4));
  EXPECT_EQ(kNotFound, view16_with_null.rfind(UChar(0), 3));
}

TEST(StringViewTest, RfindSubstring) {
  EXPECT_EQ(0u, StringView().rfind(""));
  EXPECT_EQ(0u, StringView("").rfind(""));
  EXPECT_EQ(0u, StringView().rfind(StringView()));
  EXPECT_EQ(0u, StringView("").rfind(StringView()));
  EXPECT_EQ(3u, StringView("abc").rfind(""));
  EXPECT_EQ(3u, StringView("abc").rfind(StringView()));
  EXPECT_EQ(3u, StringView("abcdef").rfind("def", 3u));
  EXPECT_EQ(StringView::npos, StringView("abcdef").rfind("def", 2u));
  EXPECT_EQ(0u, StringView("abcdef").rfind("abc", 3u));
}

TEST(StringViewTest, ContainsChar) {
  EXPECT_FALSE(StringView().contains(0));
  EXPECT_FALSE(StringView("").contains(0));
  EXPECT_FALSE(StringView(u"").contains(0));
  EXPECT_FALSE(StringView("ascii").contains(0));
  EXPECT_FALSE(StringView(u"ascii").contains(0));
  EXPECT_TRUE(StringView(base::byte_span_from_cstring("as\0cii")).contains(0));
  EXPECT_TRUE(StringView(base::span_from_cstring(u"asci\0i")).contains(0));

  EXPECT_FALSE(StringView("ascii").contains(uchar::kBlackSquare));
  EXPECT_FALSE(StringView(u"ascii").contains(uchar::kBlackSquare));
  EXPECT_TRUE(StringView(u"ascii\u25A0").contains(uchar::kBlackSquare));
}

TEST(StringViewTest, ContainsString) {
  EXPECT_TRUE(StringView().contains(""));
  EXPECT_FALSE(StringView().contains("foo"));

  EXPECT_TRUE(StringView("").contains(""));
  EXPECT_TRUE(StringView(u"").contains(""));
  EXPECT_FALSE(StringView(u"").contains("foo"));

  EXPECT_TRUE(StringView("ascii").contains(""));
  EXPECT_TRUE(StringView(u"ascii").contains(""));
  EXPECT_TRUE(
      StringView(base::byte_span_from_cstring("as\0cii")).contains("cii"));
  EXPECT_TRUE(StringView(base::span_from_cstring(u"unico\0de")).contains("de"));
  EXPECT_TRUE(StringView(base::span_from_cstring(u"unico\0de"))
                  .contains(StringView(base::span_from_cstring(u"\0"))));

  EXPECT_FALSE(StringView("ascii").contains(u"\u25A0"));
  EXPECT_FALSE(StringView(u"ascii").contains(u"\u25A0"));
  EXPECT_TRUE(StringView(u"ascii\u25A0").contains(u"\u25A0"));
}

TEST(StringViewTest, StartsWith) {
  EXPECT_TRUE(StringView().starts_with(""));
  EXPECT_TRUE(StringView().starts_with(StringView()));
  EXPECT_TRUE(StringView("").starts_with(""));
  EXPECT_TRUE(StringView("").starts_with(StringView()));
  EXPECT_TRUE(StringView("foo").starts_with("foo"));
  EXPECT_FALSE(StringView("foo").starts_with("foobar"));
  EXPECT_TRUE(StringView("foobar").starts_with("foo"));
  EXPECT_TRUE(StringView(u"foo").starts_with("foo"));
  EXPECT_TRUE(StringView("foobar").starts_with(u"foo"));
  EXPECT_FALSE(StringView("foobar").starts_with(u"bar"));
}

TEST(StringViewTest, StartsWithChar) {
  EXPECT_FALSE(StringView().starts_with(0));
  EXPECT_FALSE(StringView("").starts_with(0));
  EXPECT_TRUE(StringView("foo").starts_with('f'));
  EXPECT_FALSE(StringView("foo").starts_with(0x6666));  // 'f' == 0x66
  EXPECT_FALSE(StringView("foo").starts_with('o'));
  EXPECT_TRUE(StringView(u"foo").starts_with('f'));
  EXPECT_FALSE(StringView(u"foo").starts_with('o'));
  EXPECT_TRUE(StringView(u"\u25A0").starts_with(uchar::kBlackSquare));
  EXPECT_FALSE(StringView(u"\u25A0").starts_with(
      static_cast<LChar>(uchar::kBlackSquare)));
}

TEST(StringViewTest, EndsWith) {
  EXPECT_TRUE(StringView().ends_with(""));
  EXPECT_TRUE(StringView().ends_with(StringView()));
  EXPECT_TRUE(StringView("").ends_with(""));
  EXPECT_TRUE(StringView("").ends_with(StringView()));
  EXPECT_TRUE(StringView("foo").ends_with("foo"));
  EXPECT_FALSE(StringView("foo").ends_with("barfoo"));
  EXPECT_TRUE(StringView("foobar").ends_with("bar"));
  EXPECT_TRUE(StringView(u"foo").ends_with("foo"));
  EXPECT_TRUE(StringView("foobar").ends_with(u"bar"));
  EXPECT_FALSE(StringView("foobar").ends_with(u"foo"));
}

TEST(StringViewTest, EndsWithChar) {
  EXPECT_FALSE(StringView().ends_with(0));
  EXPECT_FALSE(StringView("").ends_with(0));
  EXPECT_TRUE(StringView("foo").ends_with('o'));
  EXPECT_FALSE(StringView("foo").ends_with(0x6f6f));  // 'o' == 0x6f
  EXPECT_FALSE(StringView("foo").ends_with('f'));
  EXPECT_TRUE(StringView(u"foo").ends_with('o'));
  EXPECT_FALSE(StringView(u"foo").ends_with('f'));
  EXPECT_TRUE(StringView(u"fo\u25A0").ends_with(uchar::kBlackSquare));
  EXPECT_FALSE(StringView(u"fo\u25A0")
                   .ends_with(static_cast<LChar>(uchar::kBlackSquare)));
}

TEST(StringViewTest, Substr) {
  StringView view8("abc");
  EXPECT_EQ(u"abc", view8.substr(0));
  EXPECT_EQ("abc", view8.substr(0));
  EXPECT_EQ("bc", view8.substr(1));
  EXPECT_EQ("c", view8.substr(2));
  EXPECT_EQ("", view8.substr(3));
  EXPECT_EQ("", view8.substr(3, 1));
  EXPECT_EQ("ab", view8.substr(0, 2));
  EXPECT_EQ("abc", view8.substr(0, 3));
  EXPECT_EQ("abc", view8.substr(0, 4));
  EXPECT_EQ("b", view8.substr(1, 1));

  StringView view16(u"abc");
  EXPECT_EQ("abc", view16.substr(0));
  EXPECT_EQ(u"abc", view16.substr(0));
  EXPECT_EQ(u"bc", view16.substr(1));
  EXPECT_EQ(u"c", view16.substr(2));
  EXPECT_EQ(u"", view16.substr(3));
  EXPECT_EQ(u"", view16.substr(3, 1));
  EXPECT_EQ(u"ab", view16.substr(0, 2));
  EXPECT_EQ(u"abc", view8.substr(0, 3));
  EXPECT_EQ(u"abc", view8.substr(0, 4));
  EXPECT_EQ(u"b", view16.substr(1, 1));
}

TEST(StringViewTest, RemovePrefix) {
  auto apply_and_return = [](StringView view, wtf_size_t len) {
    view.remove_prefix(len);
    return view;
  };
  EXPECT_TRUE(apply_and_return(StringView(), 0).IsNull());
  EXPECT_EQ("", apply_and_return(StringView(""), 0));

  EXPECT_EQ("abc", apply_and_return(StringView("abc"), 0));
  EXPECT_EQ("bc", apply_and_return(StringView("abc"), 1));
  EXPECT_EQ("c", apply_and_return(StringView("abc"), 2));
  EXPECT_EQ("", apply_and_return(StringView("abc"), 3));

  EXPECT_EQ(u"abc", apply_and_return(StringView(u"abc"), 0));
  EXPECT_EQ(u"bc", apply_and_return(StringView(u"abc"), 1));
  EXPECT_EQ(u"c", apply_and_return(StringView(u"abc"), 2));
  EXPECT_EQ(u"", apply_and_return(StringView(u"abc"), 3));
}

TEST(StringViewTest, RemoveSuffix) {
  auto apply_and_return = [](StringView view, wtf_size_t len) {
    view.remove_suffix(len);
    return view;
  };
  EXPECT_TRUE(apply_and_return(StringView(), 0).IsNull());
  EXPECT_EQ("", apply_and_return(StringView(""), 0));

  EXPECT_EQ("abc", apply_and_return(StringView("abc"), 0));
  EXPECT_EQ("ab", apply_and_return(StringView("abc"), 1));
  EXPECT_EQ("a", apply_and_return(StringView("abc"), 2));
  EXPECT_EQ("", apply_and_return(StringView("abc"), 3));

  EXPECT_EQ(u"abc", apply_and_return(StringView(u"abc"), 0));
  EXPECT_EQ(u"ab", apply_and_return(StringView(u"abc"), 1));
  EXPECT_EQ(u"a", apply_and_return(StringView(u"abc"), 2));
  EXPECT_EQ(u"", apply_and_return(StringView(u"abc"), 3));
}

TEST(StringViewTest, StripWhiteSpace) {
  StringView expected("Hello  world");
  EXPECT_EQ(expected, StringView("Hello  world").StripWhiteSpace());
  EXPECT_EQ(expected, StringView("  Hello  world  ").StripWhiteSpace());
  EXPECT_EQ(expected, StringView("\nHello  world\v  ").StripWhiteSpace());

  EXPECT_EQ(StringView(), StringView().StripWhiteSpace());
  EXPECT_EQ(StringView(""), StringView("").StripWhiteSpace());
  EXPECT_EQ(StringView(""), StringView("\n").StripWhiteSpace());
  EXPECT_EQ(StringView(""), StringView(u"\u3000").StripWhiteSpace());
}

TEST(StringViewTest, StripWhiteSpaceWithPredicate) {
  StringView expected("Hello  world");
  IsWhiteSpaceFunctionPtr p = IsASCIISpaceWHATWG;
  EXPECT_EQ(expected, StringView("Hello  world").StripWhiteSpace(p));
  EXPECT_EQ(expected, StringView("  Hello  world  ").StripWhiteSpace(p));
  EXPECT_EQ("Hello  world\v",
            StringView("\nHello  world\v  ").StripWhiteSpace(p));

  EXPECT_EQ(StringView(), StringView().StripWhiteSpace(p));
  EXPECT_EQ(StringView(""), StringView("").StripWhiteSpace(p));
  EXPECT_EQ(StringView(""), StringView("\n").StripWhiteSpace(p));
  EXPECT_EQ(StringView(u"\u3000"), StringView(u"\u3000").StripWhiteSpace(p));
}

TEST(StringViewTest, SplitByChar) {
  auto result = StringView("").SplitSkippingEmpty(' ');
  EXPECT_EQ(0u, result.size());

  result = StringView("  foo  bar").SplitSkippingEmpty(' ');
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ("foo", result[0]);
  EXPECT_EQ("bar", result[1]);

  result = StringView("").Split(',');
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ("", result[0]);

  result = StringView("foo,,bar").Split(',');
  EXPECT_EQ(3u, result.size());
  EXPECT_EQ("foo", result[0]);
  EXPECT_EQ("", result[1]);
  EXPECT_EQ("bar", result[2]);
}

}  // namespace blink
