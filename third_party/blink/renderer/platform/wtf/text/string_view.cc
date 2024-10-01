// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

#include <unicode/utf16.h>

#include "base/check.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_fast_path.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {
namespace {
class StackStringViewAllocator {
 public:
  explicit StackStringViewAllocator(
      StringView::StackBackingStore& backing_store)
      : backing_store_(backing_store) {}
  using ResultStringType = StringView;

  template <typename CharType>
  StringView Alloc(wtf_size_t length, CharType*& buffer) {
    buffer = backing_store_.Realloc<CharType>(length);
    return StringView(buffer, length);
  }

  StringView CoerceOriginal(StringView string) { return string; }

 private:
  StringView::StackBackingStore& backing_store_;
};
}  // namespace

StringView::StringView(const UChar* chars)
    : StringView(chars, chars ? LengthOfNullTerminatedString(chars) : 0) {}

#if DCHECK_IS_ON()
StringView::~StringView() {
  DCHECK(impl_);
  DCHECK(!impl_->HasOneRef() || impl_->IsStatic())
      << "StringView does not own the StringImpl, it "
         "must not have the last ref.";
}
#endif

// Helper to write a three-byte UTF-8 code point to the buffer, caller must
// check room is available.
static inline void PutUTF8Triple(char*& buffer, UChar ch) {
  DCHECK_GE(ch, 0x0800);
  *buffer++ = static_cast<char>(((ch >> 12) & 0x0F) | 0xE0);
  *buffer++ = static_cast<char>(((ch >> 6) & 0x3F) | 0x80);
  *buffer++ = static_cast<char>((ch & 0x3F) | 0x80);
}

std::string StringView::Utf8(UTF8ConversionMode mode) const {
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
    const LChar* characters = Characters8();

    unicode::ConversionResult result =
        unicode::ConvertLatin1ToUTF8(&characters, characters + length, &buffer,
                                     buffer + buffer_vector.size());
    // (length * 3) should be sufficient for any conversion
    DCHECK_NE(result, unicode::kTargetExhausted);
  } else {
    const UChar* characters = Characters16();

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
        DCHECK_EQ(characters + 1, Characters16() + length);
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

bool StringView::ContainsOnlyASCIIOrEmpty() const {
  if (StringImpl* impl = SharedImpl())
    return impl->ContainsOnlyASCIIOrEmpty();
  if (empty())
    return true;
  ASCIIStringAttributes attrs =
      Is8Bit() ? CharacterAttributes(Characters8(), length())
               : CharacterAttributes(Characters16(), length());
  return attrs.contains_only_ascii;
}

bool StringView::SubstringContainsOnlyWhitespaceOrEmpty(unsigned from,
                                                        unsigned to) const {
  SECURITY_DCHECK(from <= length());
  SECURITY_DCHECK(to <= length());
  DCHECK(from <= to);

  if (Is8Bit()) {
    for (wtf_size_t i = from; i < to; ++i) {
      if (!IsASCIISpace(Characters8()[i]))
        return false;
    }

    return true;
  }

  for (wtf_size_t i = from; i < to; ++i) {
    if (!IsASCIISpace(Characters16()[i]))
      return false;
  }

  return true;
}

String StringView::ToString() const {
  if (IsNull())
    return String();
  if (empty())
    return g_empty_string;
  if (StringImpl* impl = SharedImpl())
    return impl;
  if (Is8Bit())
    return String(Span8());
  return StringImpl::Create8BitIfPossible(Span16());
}

AtomicString StringView::ToAtomicString() const {
  if (IsNull())
    return g_null_atom;
  if (empty())
    return g_empty_atom;
  if (StringImpl* impl = SharedImpl())
    return AtomicString(impl);
  if (Is8Bit())
    return AtomicString(Span8());
  return AtomicString(Span16());
}

String StringView::EncodeForDebugging() const {
  if (IsNull()) {
    return "<null>";
  }

  StringBuilder builder;
  builder.Append('"');
  for (unsigned index = 0; index < length(); ++index) {
    // Print shorthands for select cases.
    UChar character = (*this)[index];
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

bool EqualStringView(const StringView& a, const StringView& b) {
  if (a.IsNull() || b.IsNull())
    return a.IsNull() == b.IsNull();
  if (a.length() != b.length())
    return false;
  if (a.Bytes() == b.Bytes() && a.Is8Bit() == b.Is8Bit())
    return true;
  if (a.Is8Bit()) {
    if (b.Is8Bit())
      return Equal(a.Characters8(), b.Characters8(), a.length());
    return Equal(a.Characters8(), b.Characters16(), a.length());
  }
  if (b.Is8Bit())
    return Equal(a.Characters16(), b.Characters8(), a.length());
  return Equal(a.Characters16(), b.Characters16(), a.length());
}

bool DeprecatedEqualIgnoringCaseAndNullity(const StringView& a,
                                           const StringView& b) {
  if (a.length() != b.length())
    return false;
  if (a.Is8Bit()) {
    if (b.Is8Bit()) {
      return DeprecatedEqualIgnoringCase(a.Characters8(), b.Characters8(),
                                         a.length());
    }
    return DeprecatedEqualIgnoringCase(a.Characters8(), b.Characters16(),
                                       a.length());
  }
  if (b.Is8Bit()) {
    return DeprecatedEqualIgnoringCase(a.Characters16(), b.Characters8(),
                                       a.length());
  }
  return DeprecatedEqualIgnoringCase(a.Characters16(), b.Characters16(),
                                     a.length());
}

bool DeprecatedEqualIgnoringCase(const StringView& a, const StringView& b) {
  if (a.IsNull() || b.IsNull())
    return a.IsNull() == b.IsNull();
  return DeprecatedEqualIgnoringCaseAndNullity(a, b);
}

bool EqualIgnoringASCIICase(const StringView& a, const StringView& b) {
  if (a.IsNull() || b.IsNull())
    return a.IsNull() == b.IsNull();
  if (a.length() != b.length())
    return false;
  if (a.Bytes() == b.Bytes() && a.Is8Bit() == b.Is8Bit())
    return true;
  if (a.Is8Bit()) {
    if (b.Is8Bit())
      return EqualIgnoringASCIICase(a.Characters8(), b.Characters8(),
                                    a.length());
    return EqualIgnoringASCIICase(a.Characters8(), b.Characters16(),
                                  a.length());
  }
  if (b.Is8Bit())
    return EqualIgnoringASCIICase(a.Characters16(), b.Characters8(),
                                  a.length());
  return EqualIgnoringASCIICase(a.Characters16(), b.Characters16(), a.length());
}

StringView StringView::LowerASCIIMaybeUsingBuffer(
    StackBackingStore& buffer) const {
  return ConvertASCIICase(*this, LowerConverter(),
                          StackStringViewAllocator(buffer));
}

UChar32 StringView::CodepointAt(unsigned i) const {
  SECURITY_DCHECK(i < length());
  if (Is8Bit())
    return (*this)[i];
  UChar32 codepoint;
  U16_GET(Characters16(), 0, i, length(), codepoint);
  return codepoint;
}

unsigned StringView::NextCodePointOffset(unsigned i) const {
  DCHECK_LT(i, length());
  if (Is8Bit())
    return i + 1;
  const UChar* str = Characters16() + i;
  ++i;
  if (i < length() && U16_IS_LEAD(*str++) && U16_IS_TRAIL(*str))
    ++i;
  return i;
}

CodePointIterator StringView::begin() const {
  return CodePointIterator(*this);
}

CodePointIterator StringView::end() const {
  return CodePointIterator::End(*this);
}

std::ostream& operator<<(std::ostream& out, const StringView& string) {
  return out << string.EncodeForDebugging().Utf8();
}

}  // namespace WTF
