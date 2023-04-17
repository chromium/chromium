/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SHADOW_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SHADOW_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

enum class ShadowStyle { kNormal, kInset };

// This class holds information about shadows for the text-shadow and box-shadow
// properties, as well as the drop-shadow(...) filter operation.
class CORE_EXPORT ShadowData {
  USING_FAST_MALLOC(ShadowData);

 public:
  ShadowData(gfx::PointF location,
             float blur,
             float spread,
             ShadowStyle style,
             StyleColor color,
             float opacity = 1.0f)
      : location_(location),
        blur_(blur, blur),
        spread_(spread),
        color_(color),
        style_(style),
        opacity_(opacity) {}

  ShadowData(gfx::PointF location,
             gfx::PointF blur,
             float spread,
             ShadowStyle style,
             StyleColor color,
             float opacity = 1.0f)
      : location_(location),
        blur_(blur),
        spread_(spread),
        color_(color),
        style_(style),
        opacity_(opacity) {}

  bool operator==(const ShadowData&) const;
  bool operator!=(const ShadowData& o) const { return !(*this == o); }

  static ShadowData NeutralValue();

  float X() const { return location_.x(); }
  float Y() const { return location_.y(); }
  gfx::PointF Location() const { return location_; }
  float Blur() const { return blur_.x(); }
  gfx::PointF BlurXY() const { return blur_; }
  float Spread() const { return spread_; }
  ShadowStyle Style() const { return style_; }
  StyleColor GetColor() const { return color_; }
  float Opacity() const { return opacity_; }

  void OverrideColor(Color color) { color_ = StyleColor(color); }

  // Outsets needed to adjust a source rectangle to the one cast by this
  // shadow.
  gfx::OutsetsF RectOutsets() const;

 private:
  gfx::PointF location_;
  gfx::PointF blur_;
  float spread_;
  StyleColor color_;
  ShadowStyle style_;
  float opacity_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SHADOW_DATA_H_
