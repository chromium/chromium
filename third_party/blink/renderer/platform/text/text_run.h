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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_RUN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_RUN_H_

#include <unicode/utf16.h>

#include "base/containers/span.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/tab_size.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_justify.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT TextRun final {
  DISALLOW_NEW();

 public:
  enum ExpansionBehaviorFlags {
    kForbidTrailingExpansion = 0 << 0,
    kAllowTrailingExpansion = 1 << 0,
    kForbidLeadingExpansion = 0 << 1,
    kAllowLeadingExpansion = 1 << 1,
  };

  typedef unsigned ExpansionBehavior;

  TextRun(const LChar* c,
          unsigned len,
          float xpos = 0,
          float expansion = 0,
          ExpansionBehavior expansion_behavior = kAllowTrailingExpansion |
                                                 kForbidLeadingExpansion,
          TextDirection direction = TextDirection::kLtr,
          bool directional_override = false)
      : characters_length_(len),
        len_(len),
        xpos_(xpos),
        expansion_(expansion),
        expansion_behavior_(expansion_behavior),
        is_8bit_(true),
        allow_tabs_(false),
        direction_(static_cast<unsigned>(direction)),
        directional_override_(directional_override),
        disable_spacing_(false),
        text_justify_(static_cast<unsigned>(TextJustify::kAuto)),
        normalize_space_(false),
        tab_size_(0) {
    data_.characters8 = c;
  }

  TextRun(const UChar* c,
          unsigned len,
          float xpos = 0,
          float expansion = 0,
          ExpansionBehavior expansion_behavior = kAllowTrailingExpansion |
                                                 kForbidLeadingExpansion,
          TextDirection direction = TextDirection::kLtr,
          bool directional_override = false)
      : characters_length_(len),
        len_(len),
        xpos_(xpos),
        expansion_(expansion),
        expansion_behavior_(expansion_behavior),
        is_8bit_(false),
        allow_tabs_(false),
        direction_(static_cast<unsigned>(direction)),
        directional_override_(directional_override),
        disable_spacing_(false),
        text_justify_(static_cast<unsigned>(TextJustify::kAuto)),
        normalize_space_(false),
        tab_size_(0) {
    data_.characters16 = c;
  }

  TextRun(const StringView& string,
          float xpos = 0,
          float expansion = 0,
          ExpansionBehavior expansion_behavior = kAllowTrailingExpansion |
                                                 kForbidLeadingExpansion,
          TextDirection direction = TextDirection::kLtr,
          bool directional_override = false)
      : characters_length_(string.length()),
        len_(string.length()),
        xpos_(xpos),
        expansion_(expansion),
        expansion_behavior_(expansion_behavior),
        allow_tabs_(false),
        direction_(static_cast<unsigned>(direction)),
        directional_override_(directional_override),
        disable_spacing_(false),
        text_justify_(static_cast<unsigned>(TextJustify::kAuto)),
        normalize_space_(false),
        tab_size_(0) {
    if (!characters_length_) {
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
    return Is8Bit() ? StringView(data_.characters8, len_)
                    : StringView(data_.characters16, len_);
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
  unsigned CharactersLength() const { return characters_length_; }

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
  void SetCharactersLength(unsigned characters_length) {
    characters_length_ = characters_length;
  }

  void SetExpansionBehavior(ExpansionBehavior behavior) {
    expansion_behavior_ = behavior;
  }

  bool AllowTabs() const { return allow_tabs_; }
  TabSize GetTabSize() const { return tab_size_; }
  void SetTabSize(bool, TabSize);

  float XPos() const { return xpos_; }
  void SetXPos(float x_pos) { xpos_ = x_pos; }
  float Expansion() const { return expansion_; }
  void SetExpansion(float expansion) { expansion_ = expansion; }
  bool AllowsLeadingExpansion() const {
    return expansion_behavior_ & kAllowLeadingExpansion;
  }
  bool AllowsTrailingExpansion() const {
    return expansion_behavior_ & kAllowTrailingExpansion;
  }
  TextDirection Direction() const {
    return static_cast<TextDirection>(direction_);
  }
  bool Rtl() const { return Direction() == TextDirection::kRtl; }
  bool Ltr() const { return Direction() == TextDirection::kLtr; }
  bool DirectionalOverride() const { return directional_override_; }
  bool SpacingDisabled() const { return disable_spacing_; }

  void DisableSpacing() { disable_spacing_ = true; }
  void SetDirection(TextDirection direction) {
    direction_ = static_cast<unsigned>(direction);
  }
  void SetDirectionalOverride(bool override) {
    directional_override_ = override;
  }

  void SetTextJustify(TextJustify text_justify) {
    text_justify_ = static_cast<unsigned>(text_justify);
  }
  TextJustify GetTextJustify() const {
    return static_cast<TextJustify>(text_justify_);
  }

  // Up-converts to UTF-16 as needed and normalizes spaces and Unicode control
  // characters as per the CSS Text Module Level 3 specification.
  // https://drafts.csswg.org/css-text-3/#white-space-processing
  String NormalizedUTF16() const;

 private:
  union {
    const LChar* characters8;
    const UChar* characters16;
    const void* bytes_;
  } data_;
  // Marks the end of the characters buffer.  Default equals to m_len.
  unsigned characters_length_;
  unsigned len_;

  // m_xpos is the x position relative to the left start of the text line, not
  // relative to the left start of the containing block. In the case of right
  // alignment or center alignment, left start of the text line is not the same
  // as left start of the containing block.
  float xpos_;

  float expansion_;
  ExpansionBehavior expansion_behavior_ : 2;
  unsigned is_8bit_ : 1;
  unsigned allow_tabs_ : 1;
  unsigned direction_ : 1;
  // Was this direction set by an override character.
  unsigned directional_override_ : 1;
  unsigned disable_spacing_ : 1;
  unsigned text_justify_ : 2;
  unsigned normalize_space_ : 1;
  TabSize tab_size_;
};

inline void TextRun::SetTabSize(bool allow, TabSize size) {
  allow_tabs_ = allow;
  tab_size_ = size;
}

}  // namespace blink

#endif
