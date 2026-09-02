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

#include <unicode/utf16.h>

#include <type_traits>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/integer_to_string_conversion.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace blink {

// A concept to check if a type is a character type supported by
// StringBuilder::Append().
template <typename CharType>
concept IsAppendableCharType =
    IsStringCharType<CharType> || std::is_same_v<char, CharType> ||
    std::is_same_v<UChar32, CharType>;

// A concept to check if a type is a string type supported by
// StringBuilder::Append().
template <typename T>
concept IsAppendableStringType = std::is_convertible_v<T, StringView>;

// A concept to check if a type is supported by StringBuilder::Append() or
// StringBuilder::AppendNumber().
template <typename T>
concept IsAppendableType =
    IsAppendableCharType<T> || IsAppendableStringType<T> ||
    std::is_integral_v<T> || std::is_floating_point_v<T>;

class WTF_EXPORT StringBuilder {
  USING_FAST_MALLOC(StringBuilder);

 public:
  StringBuilder() : no_buffer_() {}
  StringBuilder(const StringBuilder&) = delete;
  StringBuilder& operator=(const StringBuilder&) = delete;
  ~StringBuilder() { ClearBuffer(); }

  bool DoesAppendCauseOverflow(unsigned length) const;

  void Append(base::span<const UChar> chars);
  void Append(base::span<const LChar> chars);

  void Append(const StringBuilder& other) {
    if (!other.length_)
      return;

    if (!length_ && !HasBuffer() && !other.string_.IsNull()) {
      string_ = other.string_;
      length_ = other.length_;
      is_8bit_ = other.is_8bit_;
      return;
    }

    if (other.Is8Bit())
      Append(other.Span8());
    else
      Append(other.Span16());
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

  void Append(const StringView& view) {
    if (view.empty()) {
      return;
    }

    // If we're appending to an empty builder, and there is not a buffer
    // (reserveCapacity has not been called), then retain a view into the
    // source impl rather than copying its characters.
    //
    // This is important to avoid string copies inside dom operations like
    // Node::textContent when there's only a single Text node child, or
    // inside the parser in the common case when flushing buffered text to
    // a Text node. It also seeds the view that the span append paths below
    // can later extend (see TryExtendSharedView) so that a buffer collected
    // verbatim from a single source (e.g. inline `text_content`) is never
    // materialized as a copy.
    //
    // We only retain when the view starts at the beginning of its impl (a
    // prefix). A view starting partway in could never be returned as the
    // shared impl anyway (ToString() would substring-copy it), and extension
    // only grows the view forward, so restricting to prefixes loses no
    // sharing while letting us track just a length (no offset).
    if (!length_ && !HasBuffer()) {
      if (StringImpl* impl = view.SharedSubImpl()) {
        string_ = String(impl);
        length_ = view.length();
        is_8bit_ = impl->Is8Bit();
        return;
      }
    }

    // Otherwise append the characters. When we already hold a shared view, the
    // span append paths try to extend it in place instead of copying.
    if (view.Is8Bit()) {
      Append(view.Span8());
    } else {
      Append(view.Span16());
    }
  }

  void Append(UChar c) {
    if (is_8bit_ && c <= 0xFF) {
      Append(static_cast<LChar>(c));
      return;
    }
    if (!is_8bit_ && TryExtendSharedViewWithChar(string_.Span16(), c)) {
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
    if (TryExtendSharedViewWithChar(string_.Span8(), c)) {
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
    Append(converter.Span());
  }

  void AppendNumber(bool);

  void AppendNumber(float);

  void AppendNumber(double, unsigned precision = 6);

  // Append each elements in a collection `range`, separated by `delimiter`.
  // This adds nothing if `range` is empty.
  //
  // This supports collections of which element type is supported by
  // StringBuilder::Append() or StringBuilder::AppendNumber().
  template <typename R>
    requires(std::ranges::range<R> &&
             IsAppendableType<std::ranges::range_value_t<R>>)
  StringBuilder& AppendRange(const R& range, StringView delimiter) {
    StringView current_delimiter;
    for (const auto& item : range) {
      Append(current_delimiter);
      current_delimiter = delimiter;
      if constexpr (IsAppendableCharType<typename R::value_type>) {
        Append(item);
      } else if constexpr (std::is_integral_v<typename R::value_type> ||
                           std::is_floating_point_v<typename R::value_type>) {
        AppendNumber(item);
      } else {
        Append(item);
      }
    }
    return *this;
  }

  // Append each elements in a collection `range`, separated by `delimiter`.
  // This adds nothing if `range` is empty.  `stringifier` is a callable object,
  // and it should append an element to the StringBuilder.
  //
  // Example:
  //   HeapVector<Member<Foo>> list;
  //   StringBuilder builder;
  //   builder.AppendRange(
  //       list, ", ", [](const auto& value, StringBuilder& builder) {
  //         builder.Append(value->ToString());
  //       });
  template <typename R, typename F>
    requires(std::ranges::range<R> &&
             std::invocable<F,
                            const std::ranges::range_value_t<R>&,
                            StringBuilder&> &&
             std::is_void_v<
                 std::invoke_result_t<F,
                                      const std::ranges::range_value_t<R>&,
                                      StringBuilder&>>)
  StringBuilder& AppendRange(const R& range,
                             StringView delimiter,
                             F stringifier) {
    StringView current_delimiter;
    for (const auto& item : range) {
      Append(current_delimiter);
      current_delimiter = delimiter;
      stringifier(item, *this);
    }
    return *this;
  }

  void erase(unsigned);

  // ReleaseString is similar to ToString but releases the string_ object
  // to the caller, preventing refcount trashing. Prefer it over ToString()
  // if the StringBuilder is going to be destroyed or cleared afterwards.
  String ReleaseString();
  String ToString();
  AtomicString ToAtomicString();
  String Substring(unsigned start, unsigned length) const;
  StringView SubstringView(unsigned start, unsigned length) const;

  operator StringView() const {
    if (Is8Bit()) {
      return StringView(Span8());
    } else {
      return StringView(Span16());
    }
  }

  unsigned length() const { return length_; }
  bool empty() const { return !length_; }

  unsigned Capacity() const;
  // Increase the capacity of the backing buffer to at least |new_capacity|. The
  // behavior is the same as |Vector::ReserveCapacity|:
  // * Increase the capacity even when there are existing characters or a
  //   capacity.
  // * The characters in the backing buffer are not affected.
  // * This function does not shrink the size of the backing buffer, even if
  //   |new_capacity| is small.
  // * This function may cause a reallocation.
  void ReserveCapacity(unsigned new_capacity);
  // This is analogous to |Ensure16Bit| and |ReserveCapacity|, but can avoid
  // double reallocations when the current buffer is 8 bits and is smaller than
  // |new_capacity|.
  void Reserve16BitCapacity(unsigned new_capacity);

  // TODO(esprehn): Rename to shrink().
  void Resize(unsigned new_size);

  UChar operator[](unsigned i) const {
    if (is_8bit_)
      return Span8()[i];
    return Span16()[i];
  }

  base::span<const LChar> Span8() const {
    DCHECK(is_8bit_);
    if (!length()) {
      return {};
    }
    if (!string_.IsNull()) {
      return string_.Span8().first(length_);
    }
    DCHECK(has_buffer_);
    return base::span(buffer8_).first(length());
  }

  base::span<const UChar> Span16() const {
    DCHECK(!is_8bit_);
    if (!length()) {
      return {};
    }
    if (!string_.IsNull()) {
      return string_.Span16().first(length_);
    }
    DCHECK(has_buffer_);
    return base::span(buffer16_).first(length());
  }

  bool Is8Bit() const { return is_8bit_; }
  void Ensure16Bit();

  void Clear();
  void Swap(StringBuilder&);

 private:
  static const unsigned kInlineBufferSize = 256;
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

  // True when `string_` is the source of a sub-range view (rather than holding
  // exactly the builder's content), i.e. the content is the [0, length_) prefix
  // of `string_` and `length_` is shorter than the whole impl.
  bool HasSharedSubview() const {
    // Retained views always start at offset 0 (see `Append(StringView)`), so a
    // view is a strict sub-range exactly when it is a truncated prefix.
    return !string_.IsNull() && length_ != string_.length();
  }

  // Collapses any sub-range view so that `string_` holds exactly the builder's
  // content and can be returned directly. A no-op (and copy-free) when the view
  // already covers the whole impl.
  void NormalizeSharedString() {
    if (HasSharedSubview()) {
      string_ = string_.substr(0, length_);
    }
  }

  // When no buffer exists and we hold a shared view into `string_`, attempts to
  // extend that view to also cover `chars` (or the single character `c`), which
  // succeeds only when the characters immediately following the view in
  // `string_` are byte-identical to what is being appended. Returns true if
  // absorbed (no copy made).
  template <typename CharType>
  bool TryExtendSharedView(base::span<const CharType> buffer,
                           base::span<const CharType> chars);
  template <typename CharType>
  bool TryExtendSharedViewWithChar(base::span<const CharType> buffer,
                                   CharType c);

  template <typename StringType>
  void BuildString() {
    if (is_8bit_)
      string_ = StringType(Span8());
    else
      string_ = StringType(Span16());
    ClearBuffer();
  }

  String string_;
  union {
    char no_buffer_;
    Buffer8 buffer8_;
    Buffer16 buffer16_;
  };
  unsigned length_ = 0;
  bool is_8bit_ = true;
  bool has_buffer_ = false;
};

template <typename CharType>
bool StringBuilder::TryExtendSharedViewWithChar(
    base::span<const CharType> buffer,
    CharType c) {
  if (HasBuffer() || string_.IsNull()) {
    return false;
  }
  const size_t end = length_;
  if (end >= buffer.size() || buffer[end] != c) {
    return false;
  }
  ++length_;
  return true;
}

template <typename StringType>
bool Equal(const StringBuilder& a, const StringType& b) {
  if (a.length() != b.length())
    return false;

  if (!a.length())
    return true;

  if (a.Is8Bit()) {
    if (b.Is8Bit())
      return a.Span8() == b.Span8();
    return a.Span8() == b.Span16();
  }

  if (b.Is8Bit())
    return a.Span16() == b.Span8();
  return a.Span16() == b.Span16();
}

inline bool operator==(const StringBuilder& a, const StringBuilder& b) {
  return Equal(a, b);
}
inline bool operator==(const StringBuilder& a, const String& b) {
  return Equal(a, b);
}
inline bool operator==(const String& a, const StringBuilder& b) {
  return Equal(b, a);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_BUILDER_H_
