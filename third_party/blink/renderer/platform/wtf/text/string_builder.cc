/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#include <algorithm>
#include <optional>

#include "base/strings/string_util.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/text/integer_to_string_conversion.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

String StringBuilder::ReleaseString() {
  if (!length_)
    return g_empty_string;
  if (string_.IsNull())
    BuildString<String>();
  String string = std::move(string_);
  Clear();
  return string;
}

String StringBuilder::ToString() {
  if (!length_)
    return g_empty_string;
  if (string_.IsNull())
    BuildString<String>();
  return string_;
}

AtomicString StringBuilder::ToAtomicString() {
  if (!length_)
    return g_empty_atom;
  if (string_.IsNull())
    BuildString<AtomicString>();
  return AtomicString(string_);
}

String StringBuilder::Substring(unsigned start, unsigned length) const {
  if (start >= length_)
    return g_empty_string;
  if (!string_.IsNull())
    return string_.Substring(start, length);
  length = std::min(length, length_ - start);
  if (is_8bit_)
    return String(Characters8() + start, length);
  return String(Characters16() + start, length);
}

StringView StringBuilder::SubstringView(unsigned start, unsigned length) const {
  if (start >= length_)
    return StringView();
  if (!string_.IsNull())
    return StringView(string_, start, length);
  length = std::min(length, length_ - start);
  if (is_8bit_)
    return StringView(Span8().subspan(start, length));
  return StringView(Span16().subspan(start, length));
}

void StringBuilder::Swap(StringBuilder& builder) {
  std::optional<Buffer8> buffer8;
  std::optional<Buffer16> buffer16;
  if (has_buffer_) {
    if (is_8bit_) {
      buffer8 = std::move(buffer8_);
      buffer8_.~Buffer8();
    } else {
      buffer16 = std::move(buffer16_);
      buffer16_.~Buffer16();
    }
  }

  if (builder.has_buffer_) {
    if (builder.is_8bit_) {
      new (&buffer8_) Buffer8(std::move(builder.buffer8_));
      builder.buffer8_.~Buffer8();
    } else {
      new (&buffer16_) Buffer16(std::move(builder.buffer16_));
      builder.buffer16_.~Buffer16();
    }
  }

  if (buffer8)
    new (&builder.buffer8_) Buffer8(std::move(*buffer8));
  else if (buffer16)
    new (&builder.buffer16_) Buffer16(std::move(*buffer16));

  std::swap(string_, builder.string_);
  std::swap(length_, builder.length_);
  std::swap(is_8bit_, builder.is_8bit_);
  std::swap(has_buffer_, builder.has_buffer_);
}

void StringBuilder::ClearBuffer() {
  if (!has_buffer_)
    return;
  if (is_8bit_)
    buffer8_.~Buffer8();
  else
    buffer16_.~Buffer16();
  has_buffer_ = false;
}

void StringBuilder::Ensure16Bit() {
  EnsureBuffer16(0);
}

void StringBuilder::Clear() {
  ClearBuffer();
  string_ = String();
  length_ = 0;
  is_8bit_ = true;
}

unsigned StringBuilder::Capacity() const {
  if (!HasBuffer())
    return 0;
  if (is_8bit_)
    return buffer8_.capacity();
  return buffer16_.capacity();
}

void StringBuilder::ReserveCapacity(unsigned new_capacity) {
  if (!HasBuffer()) {
    if (is_8bit_)
      CreateBuffer8(new_capacity);
    else
      CreateBuffer16(new_capacity);
    return;
  }
  if (is_8bit_)
    buffer8_.reserve(new_capacity);
  else
    buffer16_.reserve(new_capacity);
}

void StringBuilder::Reserve16BitCapacity(unsigned new_capacity) {
  if (is_8bit_ || !HasBuffer())
    CreateBuffer16(new_capacity);
  else
    buffer16_.reserve(new_capacity);
}

void StringBuilder::Resize(unsigned new_size) {
  DCHECK_LE(new_size, length_);
  string_ = string_.Left(new_size);
  length_ = new_size;
  if (HasBuffer()) {
    if (is_8bit_)
      buffer8_.resize(new_size);
    else
      buffer16_.resize(new_size);
  }
}

void StringBuilder::CreateBuffer8(unsigned added_size) {
  DCHECK(!HasBuffer());
  DCHECK(is_8bit_);
  new (&buffer8_) Buffer8;
  has_buffer_ = true;
  // createBuffer is called right before appending addedSize more bytes. We
  // want to ensure we have enough space to fit m_string plus the added
  // size.
  //
  // We also ensure that we have at least the initialBufferSize of extra space
  // for appending new bytes to avoid future mallocs for appending short
  // strings or single characters. This is a no-op if m_length == 0 since
  // initialBufferSize() is the same as the inline capacity of the vector.
  // This allows doing append(string); append('\0') without extra mallocs.
  buffer8_.ReserveInitialCapacity(length_ +
                                  std::max(added_size, InitialBufferSize()));
  length_ = 0;
  Append(string_);
  string_ = String();
}

void StringBuilder::CreateBuffer16(unsigned added_size) {
  DCHECK(is_8bit_ || !HasBuffer());
  Buffer8 buffer8;
  unsigned length = length_;
  wtf_size_t capacity = 0;
  if (has_buffer_) {
    buffer8 = std::move(buffer8_);
    buffer8_.~Buffer8();
    capacity = buffer8.capacity();
  }
  new (&buffer16_) Buffer16;
  has_buffer_ = true;
  capacity = std::max<wtf_size_t>(
      capacity, length_ + std::max<unsigned>(
                              added_size, InitialBufferSize() / sizeof(UChar)));
  // See CreateBuffer8's call to ReserveInitialCapacity for why we do this.
  buffer16_.ReserveInitialCapacity(capacity);
  is_8bit_ = false;
  length_ = 0;
  if (!buffer8.empty()) {
    Append(buffer8.data(), length);
    return;
  }
  Append(string_);
  string_ = String();
}

bool StringBuilder::DoesAppendCauseOverflow(unsigned length) const {
  unsigned new_length = length_ + length;
  if (new_length < Capacity()) {
    return false;
  }
  // Expanding the underlying vector usually doubles its capacity—unless there
  // is no current buffer, in which case `length` will become the capacity.
  if (is_8bit_) {
    return (HasBuffer() ? buffer8_.capacity() * 2 : length) >=
           Buffer8::MaxCapacity();
  }
  return (HasBuffer() ? buffer16_.capacity() * 2 : length) >=
         Buffer16::MaxCapacity();
}

void StringBuilder::Append(const UChar* characters, unsigned length) {
  if (!length)
    return;
  DCHECK(characters);

  // If there's only one char we use append(UChar) instead since it will
  // check for latin1 and avoid converting to 16bit if possible.
  if (length == 1) {
    Append(*characters);
    return;
  }

  EnsureBuffer16(length);
  buffer16_.Append(characters, length);
  length_ += length;
}

void StringBuilder::Append(const LChar* characters, unsigned length) {
  if (!length)
    return;
  DCHECK(characters);

  if (is_8bit_) {
    EnsureBuffer8(length);
    buffer8_.Append(characters, length);
    length_ += length;
    return;
  }

  EnsureBuffer16(length);
  buffer16_.Append(characters, length);
  length_ += length;
}

void StringBuilder::Append(base::span<const UChar> chars) {
  if (chars.empty()) {
    return;
  }
  DCHECK(chars.data());

  // If there's only one char we use append(UChar) instead since it will
  // check for latin1 and avoid converting to 16bit if possible.
  if (chars.size() == 1) {
    Append(chars[0]);
    return;
  }

  unsigned length = base::checked_cast<unsigned>(chars.size());
  EnsureBuffer16(length);
  buffer16_.AppendSpan(chars);
  length_ += length;
}

void StringBuilder::Append(base::span<const LChar> chars) {
  if (chars.empty()) {
    return;
  }
  DCHECK(chars.data());

  unsigned length = base::checked_cast<unsigned>(chars.size());
  if (is_8bit_) {
    EnsureBuffer8(length);
    buffer8_.AppendSpan(chars);
    length_ += length;
    return;
  }

  EnsureBuffer16(length);
  buffer16_.AppendSpan(chars);
  length_ += length;
}

void StringBuilder::AppendNumber(bool number) {
  AppendNumber(static_cast<uint8_t>(number));
}

void StringBuilder::AppendNumber(float number) {
  AppendNumber(static_cast<double>(number));
}

void StringBuilder::AppendNumber(double number, unsigned precision) {
  NumberToStringBuffer buffer;
  Append(NumberToFixedPrecisionString(number, precision, buffer));
}

void StringBuilder::AppendFormat(const char* format, ...) {
  va_list args;

  static constexpr unsigned kDefaultSize = 256;
  Vector<char, kDefaultSize> buffer(kDefaultSize);

  va_start(args, format);
  int length = base::vsnprintf(buffer.data(), kDefaultSize, format, args);
  va_end(args);
  DCHECK_GE(length, 0);

  if (length >= static_cast<int>(kDefaultSize)) {
    buffer.Grow(length + 1);
    va_start(args, format);
    length = base::vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);
  }

  DCHECK_LT(static_cast<wtf_size_t>(length), buffer.size());
  Append(reinterpret_cast<const LChar*>(buffer.data()), length);
}

void StringBuilder::erase(unsigned index) {
  if (index >= length_)
    return;

  if (is_8bit_) {
    EnsureBuffer8(0);
    buffer8_.EraseAt(index);
  } else {
    EnsureBuffer16(0);
    buffer16_.EraseAt(index);
  }
  --length_;
}

}  // namespace WTF
