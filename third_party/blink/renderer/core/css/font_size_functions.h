/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_SIZE_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_SIZE_FUNCTIONS_H_

#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;

enum ApplyMinimumFontSize {
  kDoNotApplyMinimumForFontSize,
  kApplyMinimumForFontSize
};

class FontSizeFunctions {
  STATIC_ONLY(FontSizeFunctions);

 public:
  static float GetComputedSizeFromSpecifiedSize(
      const Document*,
      float zoom_factor,
      bool is_absolute_size,
      float specified_size,
      ApplyMinimumFontSize = kApplyMinimumForFontSize);

  // Given a CSS keyword in the range (xx-small to xxx-large), this function
  // returns values from '1' to '8'.
  static unsigned KeywordSize(CSSValueID value_id) {
    DCHECK(IsValidValueID(value_id));

    if (value_id == CSSValueID::kWebkitXxxLarge)
      value_id = CSSValueID::kXxxLarge;

    return static_cast<int>(value_id) - static_cast<int>(CSSValueID::kXxSmall) +
           1;
  }

  static bool IsValidValueID(CSSValueID value_id) {
    return (value_id >= CSSValueID::kXxSmall &&
            value_id <= CSSValueID::kXxxLarge) ||
           value_id == CSSValueID::kWebkitXxxLarge;
  }

  static CSSValueID InitialValueID() { return CSSValueID::kMedium; }
  static unsigned InitialKeywordSize() { return KeywordSize(InitialValueID()); }

  // Given a keyword size in the range (1 to 8), this function will return
  // the correct font size scaled relative to the user's default (4).
  static float FontSizeForKeyword(const Document*,
                                  unsigned keyword,
                                  bool is_monospace);

  // Given a font size in pixel, this function will return legacy font size
  // between 1 and 7.
  static int LegacyFontSize(const Document*,
                            int pixel_font_size,
                            bool is_monospace);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_SIZE_FUNCTIONS_H_
