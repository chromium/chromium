/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BORDER_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BORDER_STYLE_H_

#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class BorderStyle {
  DISALLOW_NEW();
  friend class ComputedStyle;

 public:
  BorderStyle() : style_(static_cast<unsigned>(EBorderStyle::kNone)) {}

  bool NonZero() const {
    return (style_ != static_cast<unsigned>(EBorderStyle::kNone));
  }

  bool operator==(const BorderStyle& o) const { return style_ == o.style_; }

  // The default width is 3px, but if the style is none we compute a value of 0
  // (in ComputedStyle itself)
  bool VisuallyEqual(const BorderStyle& o) const {
    if (style_ == static_cast<unsigned>(EBorderStyle::kNone) &&
        o.style_ == static_cast<unsigned>(EBorderStyle::kNone))
      return true;
    if (style_ == static_cast<unsigned>(EBorderStyle::kHidden) &&
        o.style_ == static_cast<unsigned>(EBorderStyle::kHidden))
      return true;
    return *this == o;
  }

  bool operator!=(const BorderStyle& o) const { return !(*this == o); }

  EBorderStyle Style() const { return static_cast<EBorderStyle>(style_); }
  void SetStyle(EBorderStyle style) { style_ = static_cast<unsigned>(style); }

 protected:
  unsigned style_ : 4;  // EBorderStyle
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BORDER_STYLE_H_
