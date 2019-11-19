/*
 * Copyright (C) 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_BUILDER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/integer_to_string_conversion.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

class WTF_EXPORT StringBuilder {
  USING_FAST_MALLOC(StringBuilder);

 public:
  StringBuilder() : no_buffer_() {}
  ~StringBuilder() { Clear(); }

  void Append(const UChar*, unsigned length);
  void Append(const LChar*, unsigned length);

  ALWAYS_INLINE void Append(const char* characters, unsigned length) {
    Append(reinterpret_cast<const LChar*>(characters), length);
  }

  void Append(const StringBuilder& other) {
    if (!other.length_)
      return;

    if (!length_ && !HasBuffer() && !other.string_.IsNull()) {
      string_ = other.string_;
      length_ = other.string_.length();
      is_8bit_ = other.string_.Is8Bit();
      return;
    }

    if (other.Is8Bit())
      Append(other.Characters8(), other.length_);
    else
      Append(other.Characters16(), other.length_);
  }

  // NOTE: The semantics of this are different than StringView(..., offset,
  // length) in that an invalid offset or invalid length is a no-op instead of
  // an error.
  // TODO(esprehn): We should probably unify the semantics instead.
  void Append(const StringView& string, unsigned offset, unsigned length) {
    unsigned extent = offset + length;
    if (extent < offset || extent > string.length())
      return;

    // We can't do this before the above check since StringView's constructor
    // doesn't accept invalid offsets or lengths.
    Append(StringView(string, offset, length));
  }

  void Append(const StringView& string) {
    if (string.IsEmpty())
      return;

    // If we're appending to an empty builder, and there is not a buffer
    // (reserveCapacity has not been called), then share the impl if
    // possible.
    //
    // This is important to avoid string copies inside dom operations like
    // Node::textContent when there's only a single Text node child, or
    // inside the parser in the common case when flushing buffered text to
    // a Text node.
    StringImpl* impl = string.SharedImpl();
    if (!length_ && !HasBuffer() && impl) {
      string_ = impl;
      length_ = impl->length();
      is_8bit_ = impl->Is8Bit();
      return;
    }

    if (string.Is8Bit())
      Append(string.Characters8(), string.length());
    else
      Append(string.Characters16(), string.length());
  }

  void Append(UChar c) {
    if (is_8bit_ && c <= 0xFF) {
      Append(static_cast<LChar>(c));
      return;
    }
    EnsureBuffer16(1);
    buffer16_.push_back(c);
    ++length_;
  }

  void Append(LChar c) {
    if (!is_8bit_) {
      Append(static_cast<UChar>(c));
      return;
    }
    EnsureBuffer8(1);
    buffer8_.push_back(c);
    ++length_;
  }

  void Append(char c) { Append(static_cast<LChar>(c)); }

  void Append(UChar32 c) {
    if (U_IS_BMP(c)) {
      Append(static_cast<UChar>(c));
      return;
    }
    Append(U16_LEAD(c));
    Append(U16_TRAIL(c));
  }

  template <typename IntegerType>
  void AppendNumber(IntegerType number) {
    IntegerToStringConverter<IntegerType> converter(number);
    Append(converter.Characters8(), converter.length());
  }

  void AppendNumber(bool);

  void AppendNumber(float);

  void AppendNumber(double, unsigned precision = 6);

  // Like WTF::String::Format, supports Latin-1 only.
  PRINTF_FORMAT(2, 3)
  void AppendFormat(const char* format, ...);

  void erase(unsigned);

  String ToString();
  AtomicString ToAtomicString();
  String Substring(unsigned start, unsigned length) const;

  unsigned length() const { return length_; }
  bool IsEmpty() const { return !length_; }

  unsigned Capacity() const;
  void ReserveCapacity(unsigned new_capacity);

  // TODO(esprehn): Rename to shrink().
  void Resize(unsigned new_size);

  UChar operator[](unsigned i) const {
    SECURITY_DCHECK(i < length_);
    if (is_8bit_)
      return Characters8()[i];
    return Characters16()[i];
  }

  const LChar* Characters8() const {
    DCHECK(is_8bit_);
    if (!length())
      return nullptr;
    if (!string_.IsNull())
      return string_.Characters8();
    DCHECK(has_buffer_);
    return buffer8_.data();
  }

  const UChar* Characters16() const {
    DCHECK(!is_8bit_);
    if (!length())
      return nullptr;
    if (!string_.IsNull())
      return string_.Characters16();
    DCHECK(has_buffer_);
    return buffer16_.data();
  }

  bool Is8Bit() const { return is_8bit_; }
  void Ensure16Bit();

  void Clear();
  void Swap(StringBuilder&);

 private:
  static const unsigned kInlineBufferSize = 16;
  static unsigned InitialBufferSize() { return kInlineBufferSize; }

  typedef Vector<LChar, kInlineBufferSize / sizeof(LChar)> Buffer8;
  typedef Vector<UChar, kInlineBufferSize / sizeof(UChar)> Buffer16;

  void EnsureBuffer8(unsigned added_size) {
    DCHECK(is_8bit_);
    if (!HasBuffer())
      CreateBuffer8(added_size);
  }

  void EnsureBuffer16(unsigned added_size) {
    if (is_8bit_ || !HasBuffer())
      CreateBuffer16(added_size);
  }

  void CreateBuffer8(unsigned added_size);
  void CreateBuffer16(unsigned added_size);
  void ClearBuffer();
  bool HasBuffer() const { return has_buffer_; }

  String string_;
  union {
    char no_buffer_;
    Buffer8 buffer8_;
    Buffer16 buffer16_;
  };
  unsigned length_ = 0;
  bool is_8bit_ = true;
  bool has_buffer_ = false;

  DISALLOW_COPY_AND_ASSIGN(StringBuilder);
};

template <typename CharType>
bool Equal(const StringBuilder& s, const CharType* buffer, unsigned length) {
  if (s.length() != length)
    return false;

  if (s.Is8Bit())
    return Equal(s.Characters8(), buffer, length);

  return Equal(s.Characters16(), buffer, length);
}

template <typename CharType>
bool DeprecatedEqualIgnoringCase(const StringBuilder& s,
                                 const CharType* buffer,
                                 unsigned length) {
  if (s.length() != length)
    return false;

  if (s.Is8Bit())
    return DeprecatedEqualIgnoringCase(s.Characters8(), buffer, length);

  return DeprecatedEqualIgnoringCase(s.Characters16(), buffer, length);
}

// Unicode aware case insensitive string matching. Non-ASCII characters might
// match to ASCII characters. This function is rarely used to implement web
// platform features.
// This function is deprecated. We should introduce EqualIgnoringASCIICase() or
// EqualIgnoringUnicodeCase(). See crbug.com/627682
inline bool DeprecatedEqualIgnoringCase(const StringBuilder& s,
                                        const char* string) {
  return DeprecatedEqualIgnoringCase(s, reinterpret_cast<const LChar*>(string),
                                     SafeCast<wtf_size_t>(strlen(string)));
}

template <typename StringType>
bool Equal(const StringBuilder& a, const StringType& b) {
  if (a.length() != b.length())
    return false;

  if (!a.length())
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

inline bool operator==(const StringBuilder& a, const StringBuilder& b) {
  return Equal(a, b);
}
inline bool operator!=(const StringBuilder& a, const StringBuilder& b) {
  return !Equal(a, b);
}
inline bool operator==(const StringBuilder& a, const String& b) {
  return Equal(a, b);
}
inline bool operator!=(const StringBuilder& a, const String& b) {
  return !Equal(a, b);
}
inline bool operator==(const String& a, const StringBuilder& b) {
  return Equal(b, a);
}
inline bool operator!=(const String& a, const StringBuilder& b) {
  return !Equal(b, a);
}

}  // namespace WTF

using WTF::StringBuilder;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_BUILDER_H_
