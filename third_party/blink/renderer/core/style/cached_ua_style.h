/*
 * Copyright (C) 2013 Google, Inc.
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
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_CACHED_UA_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_CACHED_UA_STYLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/fill_layer.h"
#include "third_party/blink/renderer/core/style/nine_piece_image.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;

// LayoutTheme::AdjustStyle wants the background and borders
// as specified by the UA sheets, excluding any author rules.
// We use this class to cache those values during
// ApplyMatchedProperties for later use during AdjustComputedStyle.
class CachedUAStyle {
  USING_FAST_MALLOC(CachedUAStyle);
  friend class ComputedStyle;

 public:
  explicit CachedUAStyle(const ComputedStyle*);

  bool BorderColorEquals(const ComputedStyle& other) const;
  bool BorderWidthEquals(const ComputedStyle& other) const;
  bool BorderRadiiEquals(const ComputedStyle& other) const;
  bool BorderStyleEquals(const ComputedStyle& other) const;

  LengthSize top_left_;
  LengthSize top_right_;
  LengthSize bottom_left_;
  LengthSize bottom_right_;
  Color border_left_color;
  Color border_right_color;
  Color border_top_color;
  Color border_bottom_color;
  bool border_left_color_is_current_color;
  bool border_right_color_is_current_color;
  bool border_top_color_is_current_color;
  bool border_bottom_color_is_current_color;
  unsigned border_left_style : 4;    // EBorderStyle
  unsigned border_right_style : 4;   // EBorderStyle
  unsigned border_top_style : 4;     // EBorderStyle
  unsigned border_bottom_style : 4;  // EBorderStyle
  float border_left_width;
  float border_right_width;
  float border_top_width;
  float border_bottom_width;
  NinePieceImage border_image;
  FillLayer background_layers;
  StyleColor background_color;

 private:
  DISALLOW_COPY_AND_ASSIGN(CachedUAStyle);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_CACHED_UA_STYLE_H_
