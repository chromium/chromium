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

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <locale.h>
#include <stdarg.h>
#include <algorithm>
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {

// Construct a string with UTF-16 data.
String::String(const UChar* characters, unsigned length)
    : impl_(characters ? StringImpl::Create(characters, length) : nullptr) {}

// Construct a string with UTF-16 data, from a null-terminated source.
String::String(const UChar* str) {
  if (!str)
    return;
  impl_ = StringImpl::Create(str, LengthOfNullTerminatedString(str));
}

// Construct a string with latin1 data.
String::String(const LChar* characters, unsigned length)
    : impl_(characters ? StringImpl::Create(characters, length) : nullptr) {}

String::String(const char* characters, unsigned length)
    : impl_(characters
                ? StringImpl::Create(reinterpret_cast<const LChar*>(characters),
                                     length)
                : nullptr) {}

#if defined(ARCH_CPU_64_BITS)
String::String(const char* characters, size_t length)
    : String(characters, SafeCast<unsigned>(length)) {}
#endif  // defined(ARCH_CPU_64_BITS)

int CodeUnitCompare(const String& a, const String& b) {
  return CodeUnitCompare(a.Impl(), b.Impl());
}

int CodeUnitCompareIgnoringASCIICase(const String& a, const char* b) {
  return CodeUnitCompareIgnoringASCIICase(a.Impl(),
                                          reinterpret_cast<const LChar*>(b));
}

UChar32 String::CharacterStartingAt(unsigned i) const {
  if (!impl_ || i >= impl_->length())
    return 0;
  return impl_->CharacterStartingAt(i);
}

void String::Ensure16Bit() {
  if (IsNull())
    return;
  if (!Is8Bit())
    return;
  if (unsigned length = this->length())
    impl_ = Make16BitFrom8BitSource(impl_->Characters8(), length).ReleaseImpl();
  else
    impl_ = StringImpl::empty16_bit_;
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
#if !defined(OS_ANDROID)
  DCHECK_EQ(strcmp(setlocale(LC_NUMERIC, NULL), "C"), 0);
#endif  // !OS_ANDROID

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
  if (IsNull())
    return "<null>";

  StringBuilder builder;
  builder.Append('"');
  for (unsigned index = 0; index < length(); ++index) {
    // Print shorthands for select cases.
    UChar character = (*impl_)[index];
    switch (character) {
      case '\t':
        builder.Append("\\t");
        break;
      case '\n':
        builder.Append("\\n");
        break;
      case '\r':
        builder.Append("\\r");
        break;
      case '"':
        builder.Append("\\\"");
        break;
      case '\\':
        builder.Append("\\\\");
        break;
      default:
        if (IsASCIIPrintable(character)) {
          builder.Append(static_cast<char>(character));
        } else {
          // Print "\uXXXX" for control or non-ASCII characters.
          builder.AppendFormat("\\u%04X", character);
        }
        break;
    }
  }
  builder.Append('"');
  return builder.ToString();
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
  return impl_->ToInt(NumberParsingOptions::kStrict, ok);
}

unsigned String::ToUIntStrict(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToUInt(NumberParsingOptions::kStrict, ok);
}

unsigned String::HexToUIntStrict(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->HexToUIntStrict(ok);
}

int64_t String::ToInt64Strict(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToInt64(NumberParsingOptions::kStrict, ok);
}

uint64_t String::ToUInt64Strict(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToUInt64(NumberParsingOptions::kStrict, ok);
}

int String::ToInt(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToInt(NumberParsingOptions::kLoose, ok);
}

unsigned String::ToUInt(bool* ok) const {
  if (!impl_) {
    if (ok)
      *ok = false;
    return 0;
  }
  return impl_->ToUInt(NumberParsingOptions::kLoose, ok);
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

String String::IsolatedCopy() const {
  if (!impl_)
    return String();
  return impl_->IsolatedCopy();
}

bool String::IsSafeToSendToAnotherThread() const {
  return !impl_ || impl_->IsSafeToSendToAnotherThread();
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
  if (this->Is8Bit()) {
    const LChar* characters = this->Characters8();

    for (unsigned i = 0; i < length; ++i) {
      LChar ch = characters[i];
      ascii[i] = ch && (ch < 0x20 || ch > 0x7f) ? '?' : ch;
    }
    return ascii;
  }

  const UChar* characters = this->Characters16();
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
    return std::string(reinterpret_cast<const char*>(this->Characters8()),
                       length);
  }

  const UChar* characters = this->Characters16();
  std::string latin1(length, '\0');
  for (unsigned i = 0; i < length; ++i) {
    UChar ch = characters[i];
    latin1[i] = ch > 0xff ? '?' : static_cast<char>(ch);
  }

  return latin1;
}

// Helper to write a three-byte UTF-8 code point to the buffer, caller must
// check room is available.
static inline void PutUTF8Triple(char*& buffer, UChar ch) {
  DCHECK_GE(ch, 0x0800);
  *buffer++ = static_cast<char>(((ch >> 12) & 0x0F) | 0xE0);
  *buffer++ = static_cast<char>(((ch >> 6) & 0x3F) | 0x80);
  *buffer++ = static_cast<char>((ch & 0x3F) | 0x80);
}

std::string String::Utf8(UTF8ConversionMode mode) const {
  unsigned length = this->length();

  if (!length)
    return std::string();

  // Allocate a buffer big enough to hold all the characters
  // (an individual UTF-16 UChar can only expand to 3 UTF-8 bytes).
  // Optimization ideas, if we find this function is hot:
  //  * We could speculatively create a std::string to contain 'length'
  //    characters, and resize if necessary (i.e. if the buffer contains
  //    non-ascii characters). (Alternatively, scan the buffer first for
  //    ascii characters, so we know this will be sufficient).
  //  * We could allocate a std::string with an appropriate size to
  //    have a good chance of being able to write the string into the
  //    buffer without reallocing (say, 1.5 x length).
  if (length > std::numeric_limits<unsigned>::max() / 3)
    return std::string();
  Vector<char, 1024> buffer_vector(length * 3);

  char* buffer = buffer_vector.data();

  if (Is8Bit()) {
    const LChar* characters = this->Characters8();

    unicode::ConversionResult result =
        unicode::ConvertLatin1ToUTF8(&characters, characters + length, &buffer,
                                     buffer + buffer_vector.size());
    // (length * 3) should be sufficient for any conversion
    DCHECK_NE(result, unicode::kTargetExhausted);
  } else {
    const UChar* characters = this->Characters16();

    if (mode == kStrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD) {
      const UChar* characters_end = characters + length;
      char* buffer_end = buffer + buffer_vector.size();
      while (characters < characters_end) {
        // Use strict conversion to detect unpaired surrogates.
        unicode::ConversionResult result = unicode::ConvertUTF16ToUTF8(
            &characters, characters_end, &buffer, buffer_end, true);
        DCHECK_NE(result, unicode::kTargetExhausted);
        // Conversion fails when there is an unpaired surrogate.  Put
        // replacement character (U+FFFD) instead of the unpaired
        // surrogate.
        if (result != unicode::kConversionOK) {
          DCHECK_LE(0xD800, *characters);
          DCHECK_LE(*characters, 0xDFFF);
          // There should be room left, since one UChar hasn't been
          // converted.
          DCHECK_LE(buffer + 3, buffer_end);
          PutUTF8Triple(buffer, kReplacementCharacter);
          ++characters;
        }
      }
    } else {
      bool strict = mode == kStrictUTF8Conversion;
      unicode::ConversionResult result =
          unicode::ConvertUTF16ToUTF8(&characters, characters + length, &buffer,
                                      buffer + buffer_vector.size(), strict);
      // (length * 3) should be sufficient for any conversion
      DCHECK_NE(result, unicode::kTargetExhausted);

      // Only produced from strict conversion.
      if (result == unicode::kSourceIllegal) {
        DCHECK(strict);
        return std::string();
      }

      // Check for an unconverted high surrogate.
      if (result == unicode::kSourceExhausted) {
        if (strict)
          return std::string();
        // This should be one unpaired high surrogate. Treat it the same
        // was as an unpaired high surrogate would have been handled in
        // the middle of a string with non-strict conversion - which is
        // to say, simply encode it to UTF-8.
        DCHECK_EQ(characters + 1, this->Characters16() + length);
        DCHECK_GE(*characters, 0xD800);
        DCHECK_LE(*characters, 0xDBFF);
        // There should be room left, since one UChar hasn't been
        // converted.
        DCHECK_LE(buffer + 3, buffer + buffer_vector.size());
        PutUTF8Triple(buffer, *characters);
      }
    }
  }

  return std::string(buffer_vector.data(), buffer - buffer_vector.data());
}

String String::Make8BitFrom16BitSource(const UChar* source, wtf_size_t length) {
  if (!length)
    return g_empty_string;

  LChar* destination;
  String result = String::CreateUninitialized(length, destination);

  CopyLCharsFromUCharSource(destination, source, length);

  return result;
}

String String::Make16BitFrom8BitSource(const LChar* source, wtf_size_t length) {
  if (!length)
    return g_empty_string16_bit;

  UChar* destination;
  String result = String::CreateUninitialized(length, destination);

  StringImpl::CopyChars(destination, source, length);

  return result;
}

String String::FromUTF8(const LChar* string_start, size_t string_length) {
  wtf_size_t length = SafeCast<wtf_size_t>(string_length);

  if (!string_start)
    return String();

  if (!length)
    return g_empty_string;

  if (CharactersAreAllASCII(string_start, length))
    return StringImpl::Create(string_start, length);

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

String String::FromUTF8(base::StringPiece s) {
  return FromUTF8(reinterpret_cast<const LChar*>(s.data()), s.size());
}

String String::FromUTF8WithLatin1Fallback(const LChar* string, size_t size) {
  String utf8 = FromUTF8(string, size);
  if (!utf8)
    return String(string, SafeCast<wtf_size_t>(size));
  return utf8;
}

std::ostream& operator<<(std::ostream& out, const String& string) {
  return out << string.EncodeForDebugging().Utf8();
}

#ifndef NDEBUG
void String::Show() const {
  DLOG(INFO) << *this;
}
#endif

}  // namespace WTF
