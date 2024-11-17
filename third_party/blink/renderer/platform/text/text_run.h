/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2011 Apple Inc. All rights reserved.
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
 *
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_RUN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_RUN_H_

#include <unicode/utf16.h>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT TextRun final {
  DISALLOW_NEW();

 public:
  TextRun(const LChar* c,
          unsigned len,
          TextDirection direction = TextDirection::kLtr,
          bool directional_override = false)
      : len_(len),
        is_8bit_(true),
        direction_(static_cast<unsigned>(direction)),
        directional_override_(directional_override),
        normalize_space_(false) {
    data_.characters8 = c;
  }

  TextRun(const UChar* c,
          unsigned len,
          TextDirection direction = TextDirection::kLtr,
          bool directional_override = false)
      : len_(len),
        is_8bit_(false),
        direction_(static_cast<unsigned>(direction)),
        directional_override_(directional_override),
        normalize_space_(false) {
    data_.characters16 = c;
  }

  TextRun(const StringView& string,
          TextDirection direction = TextDirection::kLtr,
          bool directional_override = false)
      : len_(string.length()),
        direction_(static_cast<unsigned>(direction)),
        directional_override_(directional_override),
        normalize_space_(false) {
    if (!len_) {
      is_8bit_ = true;
      data_.characters8 = nullptr;
    } else if (string.Is8Bit()) {
      data_.characters8 = string.Characters8();
      is_8bit_ = true;
    } else {
      data_.characters16 = string.Characters16();
      is_8bit_ = false;
    }
  }

  TextRun SubRun(unsigned start_offset, unsigned length) const {
    DCHECK_LT(start_offset, len_);

    TextRun result = *this;

    if (Is8Bit()) {
      result.SetText(Data8(start_offset), length);
      return result;
    }
    result.SetText(Data16(start_offset), length);
    return result;
  }

  // Returns the start index of a sub run if it was created by |SubRun|.
  // std::numeric_limits<unsigned>::max() if not a sub run.
  unsigned IndexOfSubRun(const TextRun&) const;

  UChar operator[](unsigned i) const {
    SECURITY_DCHECK(i < len_);
    return Is8Bit() ? data_.characters8[i] : data_.characters16[i];
  }
  const LChar* Data8(unsigned i) const {
    SECURITY_DCHECK(i < len_);
    DCHECK(Is8Bit());
    return &data_.characters8[i];
  }
  const UChar* Data16(unsigned i) const {
    SECURITY_DCHECK(i < len_);
    DCHECK(!Is8Bit());
    return &data_.characters16[i];
  }

  // Prefer Span8() and Span16() to Characters8() and Characters16().
  base::span<const LChar> Span8() const {
    DCHECK(Is8Bit());
    return {data_.characters8, len_};
  }
  base::span<const UChar> Span16() const {
    DCHECK(!Is8Bit());
    return {data_.characters16, len_};
  }

  const LChar* Characters8() const {
    DCHECK(Is8Bit());
    return data_.characters8;
  }
  const UChar* Characters16() const {
    DCHECK(!Is8Bit());
    return data_.characters16;
  }

  StringView ToStringView() const {
    return Is8Bit() ? StringView(Span8()) : StringView(Span16());
  }

  UChar32 CodepointAt(unsigned i) const {
    SECURITY_DCHECK(i < len_);
    if (Is8Bit())
      return (*this)[i];
    UChar32 codepoint;
    U16_GET(Characters16(), 0, i, len_, codepoint);
    return codepoint;
  }

  UChar32 CodepointAtAndNext(unsigned& i) const {
    SECURITY_DCHECK(i < len_);
    if (Is8Bit())
      return (*this)[i++];
    UChar32 codepoint;
    U16_NEXT(Characters16(), i, len_, codepoint);
    return codepoint;
  }

  const void* Bytes() const { return data_.bytes_; }

  bool Is8Bit() const { return is_8bit_; }
  unsigned length() const { return len_; }

  bool NormalizeSpace() const { return normalize_space_; }
  void SetNormalizeSpace(bool normalize_space) {
    normalize_space_ = normalize_space;
  }

  void SetText(const LChar* c, unsigned len) {
    data_.characters8 = c;
    len_ = len;
    is_8bit_ = true;
  }
  void SetText(const UChar* c, unsigned len) {
    data_.characters16 = c;
    len_ = len;
    is_8bit_ = false;
  }
  void SetText(const String&);

  TextDirection Direction() const {
    return static_cast<TextDirection>(direction_);
  }
  bool Rtl() const { return Direction() == TextDirection::kRtl; }
  bool Ltr() const { return Direction() == TextDirection::kLtr; }
  bool DirectionalOverride() const { return directional_override_; }
  void SetDirection(TextDirection direction) {
    direction_ = static_cast<unsigned>(direction);
  }
  void SetDirectionFromText();
  void SetDirectionalOverride(bool override) {
    directional_override_ = override;
  }

  // Up-converts to UTF-16 as needed and normalizes spaces and Unicode control
  // characters as per the CSS Text Module Level 3 specification.
  // https://drafts.csswg.org/css-text-3/#white-space-processing
  String NormalizedUTF16() const;

 private:
  union {
    // RAW_PTR_EXCLUSION: #union
    RAW_PTR_EXCLUSION const LChar* characters8;
    RAW_PTR_EXCLUSION const UChar* characters16;
    RAW_PTR_EXCLUSION const void* bytes_;
  } data_;
  unsigned len_;

  unsigned is_8bit_ : 1;
  unsigned direction_ : 1;
  // Was this direction set by an override character.
  unsigned directional_override_ : 1;
  unsigned normalize_space_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_RUN_H_
