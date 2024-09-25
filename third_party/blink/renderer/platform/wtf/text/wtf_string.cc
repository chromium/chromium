/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <locale.h>
#include <stdarg.h>

#include <algorithm>
#include <string_view>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/copy_lchars_from_uchar_source.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace WTF {

ASSERT_SIZE(String, void*);

// Construct a string with UTF-16 data.
String::String(base::span<const UChar> utf16_data)
    : impl_(utf16_data.data() ? StringImpl::Create(utf16_data) : nullptr) {}

String::String(const UChar* characters, unsigned length)
    : impl_(characters ? StringImpl::Create(characters, length) : nullptr) {}

// Construct a string with UTF-16 data, from a null-terminated source.
String::String(const UChar* str) {
  if (!str)
    return;
  impl_ = StringImpl::Create(str, LengthOfNullTerminatedString(str));
}

// Construct a string with latin1 data.
String::String(base::span<const LChar> latin1_data)
    : impl_(latin1_data.data() ? StringImpl::Create(latin1_data) : nullptr) {}

String::String(const LChar* characters, unsigned length)
    : impl_(characters ? StringImpl::Create(characters, length) : nullptr) {}

String::String(const char* characters, unsigned length)
    : impl_(characters
                ? StringImpl::Create(reinterpret_cast<const LChar*>(characters),
                                     length)
                : nullptr) {}

#if defined(ARCH_CPU_64_BITS)
String::String(const UChar* characters, size_t length)
    : String(characters, base::checked_cast<unsigned>(length)) {}

String::String(const char* characters, size_t length)
    : String(characters, base::checked_cast<unsigned>(length)) {}
#endif  // defined(ARCH_CPU_64_BITS)

int CodeUnitCompare(const String& a, const String& b) {
  return CodeUnitCompare(a.Impl(), b.Impl());
}

int CodeUnitCompareIgnoringASCIICase(const String& a, const char* b) {
  return CodeUnitCompareIgnoringASCIICase(a.Impl(),
                                          reinterpret_cast<const LChar*>(b));
}

wtf_size_t String::Find(base::RepeatingCallback<bool(UChar)> match_callback,
                        wtf_size_t index) const {
  return impl_ ? impl_->Find(match_callback, index) : kNotFound;
}

UChar32 String::CharacterStartingAt(unsigned i) const {
  if (!impl_ || i >= impl_->length())
    return 0;
  return impl_->CharacterStartingAt(i);
}

CodePointIterator String::begin() const {
  return CodePointIterator(*this);
}

CodePointIterator String::end() const {
  return CodePointIterator::End(*this);
}

void String::Ensure16Bit() {
  if (IsNull())
    return;
  if (!Is8Bit())
    return;
  if (!empty()) {
    impl_ = Make16BitFrom8BitSource(impl_->Span8()).ReleaseImpl();
  } else {
    impl_ = StringImpl::empty16_bit_;
  }
}

void String::Truncate(unsigned length) {
  if (impl_)
    impl_ = impl_->Truncate(length);
}

void String::Remove(unsigned start, unsigned length_to_remove) {
  if (impl_)
    impl_ = impl_->Remove(start, length_to_remove);
}

String String::Substring(unsigned pos, unsigned len) const {
  if (!impl_)
    return String();
  return impl_->Substring(pos, len);
}

String String::DeprecatedLower() const {
  if (!impl_)
    return String();
  return CaseMap::FastToLowerInvariant(impl_.get());
}

String String::LowerASCII() const {
  if (!impl_)
    return String();
  return impl_->LowerASCII();
}

String String::UpperASCII() const {
  if (!impl_)
    return String();
  return impl_->UpperASCII();
}

unsigned String::LengthWithStrippedWhiteSpace() const {
  if (!impl_) {
    return 0;
  }
  return impl_->LengthWithStrippedWhiteSpace();
}

String String::StripWhiteSpace() const {
  if (!impl_)
    return String();
  return impl_->StripWhiteSpace();
}

String String::StripWhiteSpace(IsWhiteSpaceFunctionPtr is_white_space) const {
  if (!impl_)
    return String();
  return impl_->StripWhiteSpace(is_white_space);
}

String String::SimplifyWhiteSpace(StripBehavior strip_behavior) const {
  if (!impl_)
    return String();
  return impl_->SimplifyWhiteSpace(strip_behavior);
}

String String::SimplifyWhiteSpace(IsWhiteSpaceFunctionPtr is_white_space,
                                  StripBehavior strip_behavior) const {
  if (!impl_)
    return String();
  return impl_->SimplifyWhiteSpace(is_white_space, strip_behavior);
}

String String::RemoveCharacters(CharacterMatchFunctionPtr find_match) const {
  if (!impl_)
    return String();
  return impl_->RemoveCharacters(find_match);
}

String String::FoldCase() const {
  if (!impl_)
    return String();
  return impl_->FoldCase();
}

String String::Format(const char* format, ...) {
  // vsnprintf is locale sensitive when converting floats to strings
  // and we need it to always use a decimal point. Double check that
  // the locale is compatible, and also that it is the default "C"
  // locale so that we aren't just lucky. Android's locales work
  // differently so can't check the same way there.
  DCHECK_EQ(strcmp(localeconv()->decimal_point, "."), 0);
#if !BUILDFLAG(IS_ANDROID)
  DCHECK_EQ(strcmp(setlocale(LC_NUMERIC, NULL), "C"), 0);
#endif  // !BUILDFLAG(IS_ANDROID)

  va_list args;

  // TODO(esprehn): base uses 1024, maybe we should use a bigger size too.
  static const unsigned kDefaultSize = 256;
  Vector<char, kDefaultSize> buffer(kDefaultSize);

  va_start(args, format);
  int length = base::vsnprintf(buffer.data(), buffer.size(), format, args);
  va_end(args);

  // TODO(esprehn): This can only happen if there's an encoding error, what's
  // the locale set to inside blink? Can this happen? We should probably CHECK
  // instead.
  if (length < 0)
    return String();

  if (static_cast<unsigned>(length) >= buffer.size()) {
    // vsnprintf doesn't include the NUL terminator in the length so we need to
    // add space for it when growing.
    buffer.Grow(length + 1);

    // We need to call va_end() and then va_start() each time we use args, as
    // the contents of args is undefined after the call to vsnprintf according
    // to http://man.cx/snprintf(3)
    //
    // Not calling va_end/va_start here happens to work on lots of systems, but
    // fails e.g. on 64bit Linux.
    va_start(args, format);
    length = base::vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);
  }

  CHECK_LT(static_cast<unsigned>(length), buffer.size());
  return String(reinterpret_cast<const LChar*>(buffer.data()), length);
}

String String::EncodeForDebugging() const {
  return StringView(*this).EncodeForDebugging();
}

String String::Number(float number) {
  return Number(static_cast<double>(number));
}

String String::Number(double number, unsigned precision) {
  NumberToStringBuffer buffer;
  return String(NumberToFixedPrecisionString(number, precision, buffer));
}

String String::NumberToStringECMAScript(double number) {
  NumberToStringBuffer buffer;
  return String(NumberToString(number, buffer));
}

String String::NumberToStringFixedWidth(double number,
                                        unsigned decimal_places) {
  NumberToStringBuffer buffer;
  return String(NumberToFixedWidthString(number, decimal_places, buffer));
}

int String::ToIntStrict(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToInt(NumberParsingOptions::Strict(), ok);
}

unsigned String::ToUIntStrict(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToUInt(NumberParsingOptions::Strict(), ok);
}

unsigned String::HexToUIntStrict(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->HexToUIntStrict(ok);
}

uint64_t String::HexToUInt64Strict(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->HexToUInt64Strict(ok);
}

int64_t String::ToInt64Strict(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToInt64(NumberParsingOptions::Strict(), ok);
}

uint64_t String::ToUInt64Strict(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToUInt64(NumberParsingOptions::Strict(), ok);
}

int String::ToInt(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToInt(NumberParsingOptions::Loose(), ok);
}

unsigned String::ToUInt(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToUInt(NumberParsingOptions::Loose(), ok);
}

double String::ToDouble(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0.0;
  }
  return impl_->ToDouble(ok);
}

float String::ToFloat(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0.0f;
  }
  return impl_->ToFloat(ok);
}

void String::Split(const StringView& separator,
                   bool allow_empty_entries,
                   Vector<String>& result) const {
  result.clear();

  unsigned start_pos = 0;
  wtf_size_t end_pos;
  while ((end_pos = Find(separator, start_pos)) != kNotFound) {
    if (allow_empty_entries || start_pos != end_pos)
      result.push_back(Substring(start_pos, end_pos - start_pos));
    start_pos = end_pos + separator.length();
  }
  if (allow_empty_entries || start_pos != length())
    result.push_back(Substring(start_pos));
}

void String::Split(UChar separator,
                   bool allow_empty_entries,
                   Vector<String>& result) const {
  result.clear();

  unsigned start_pos = 0;
  wtf_size_t end_pos;
  while ((end_pos = find(separator, start_pos)) != kNotFound) {
    if (allow_empty_entries || start_pos != end_pos)
      result.push_back(Substring(start_pos, end_pos - start_pos));
    start_pos = end_pos + 1;
  }
  if (allow_empty_entries || start_pos != length())
    result.push_back(Substring(start_pos));
}

std::string String::Ascii() const {
  // Printable ASCII characters 32..127 and the null character are
  // preserved, characters outside of this range are converted to '?'.

  unsigned length = this->length();
  if (!length)
    return std::string();

  std::string ascii(length, '\0');
  if (Is8Bit()) {
    const LChar* characters = Characters8();

    for (unsigned i = 0; i < length; ++i) {
      LChar ch = characters[i];
      ascii[i] = ch && (ch < 0x20 || ch > 0x7f) ? '?' : ch;
    }
    return ascii;
  }

  const UChar* characters = Characters16();
  for (unsigned i = 0; i < length; ++i) {
    UChar ch = characters[i];
    ascii[i] = ch && (ch < 0x20 || ch > 0x7f) ? '?' : static_cast<char>(ch);
  }

  return ascii;
}

std::string String::Latin1() const {
  // Basic Latin1 (ISO) encoding - Unicode characters 0..255 are
  // preserved, characters outside of this range are converted to '?'.
  unsigned length = this->length();

  if (!length)
    return std::string();

  if (Is8Bit()) {
    return std::string(reinterpret_cast<const char*>(Characters8()), length);
  }

  const UChar* characters = Characters16();
  std::string latin1(length, '\0');
  for (unsigned i = 0; i < length; ++i) {
    UChar ch = characters[i];
    latin1[i] = ch > 0xff ? '?' : static_cast<char>(ch);
  }

  return latin1;
}

String String::Make8BitFrom16BitSource(base::span<const UChar> source) {
  if (source.empty()) {
    return g_empty_string;
  }

  const wtf_size_t length = base::checked_cast<wtf_size_t>(source.size());
  base::span<LChar> destination;
  String result = String::CreateUninitialized(length, destination);

  CopyLCharsFromUCharSource(destination.data(), source.data(), length);

  return result;
}

String String::Make16BitFrom8BitSource(base::span<const LChar> source) {
  if (source.empty()) {
    return g_empty_string16_bit;
  }

  const wtf_size_t length = base::checked_cast<wtf_size_t>(source.size());
  base::span<UChar> destination;
  String result = String::CreateUninitialized(length, destination);

  StringImpl::CopyChars(destination.data(), source.data(), length);

  return result;
}

String String::FromUTF8(const LChar* string_start, size_t string_length) {
  wtf_size_t length = base::checked_cast<wtf_size_t>(string_length);

  if (!string_start)
    return String();

  if (!length)
    return g_empty_string;

  ASCIIStringAttributes attributes = CharacterAttributes(string_start, length);
  if (attributes.contains_only_ascii)
    return StringImpl::Create(string_start, length, attributes);

  Vector<UChar, 1024> buffer(length);
  UChar* buffer_start = buffer.data();

  UChar* buffer_current = buffer_start;
  const char* string_current = reinterpret_cast<const char*>(string_start);
  if (unicode::ConvertUTF8ToUTF16(
          &string_current, reinterpret_cast<const char*>(string_start + length),
          &buffer_current,
          buffer_current + buffer.size()) != unicode::kConversionOK)
    return String();

  unsigned utf16_length =
      static_cast<wtf_size_t>(buffer_current - buffer_start);
  DCHECK_LT(utf16_length, length);
  return StringImpl::Create(buffer_start, utf16_length);
}

String String::FromUTF8(const LChar* string) {
  if (!string)
    return String();
  return FromUTF8(string, strlen(reinterpret_cast<const char*>(string)));
}

String String::FromUTF8(std::string_view s) {
  return FromUTF8(reinterpret_cast<const LChar*>(s.data()), s.size());
}

String String::FromUTF8WithLatin1Fallback(const LChar* string, size_t size) {
  String utf8 = FromUTF8(string, size);
  if (!utf8)
    return String(string, base::checked_cast<wtf_size_t>(size));
  return utf8;
}

String String::FromUTF8WithLatin1Fallback(std::string_view s) {
  return FromUTF8WithLatin1Fallback(s.data(), s.size());
}

std::ostream& operator<<(std::ostream& out, const String& string) {
  return out << string.EncodeForDebugging().Utf8();
}

#ifndef NDEBUG
void String::Show() const {
  DLOG(INFO) << *this;
}
#endif

void String::WriteIntoTrace(perfetto::TracedValue context) const {
  if (!length()) {
    std::move(context).WriteString("", 0);
    return;
  }

  // Avoid the default String to StringView conversion since it calls
  // AddRef() on the StringImpl and this method is sometimes called in
  // places where that triggers DCHECKs.
  StringUTF8Adaptor adaptor(Is8Bit() ? StringView(Span8())
                                     : StringView(Span16()));
  std::move(context).WriteString(adaptor.data(), adaptor.size());
}

}  // namespace WTF
