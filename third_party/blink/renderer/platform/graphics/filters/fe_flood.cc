/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/graphics/filters/fe_flood.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/skia/include/core/SkColorFilter.h"

namespace blink {

FEFlood::FEFlood(Filter* filter,
                 const SkColor4f& flood_color,
                 float flood_opacity)
    : FilterEffect(filter),
      flood_color_(flood_color),
      flood_opacity_(flood_opacity) {
  FilterEffect::SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);
}

SkColor4f FEFlood::FloodColor() const {
  return flood_color_;
}

bool FEFlood::SetFloodColor(const SkColor4f& color) {
  if (flood_color_ == color)
    return false;
  flood_color_ = color;
  return true;
}

float FEFlood::FloodOpacity() const {
  return flood_opacity_;
}

bool FEFlood::SetFloodOpacity(float flood_opacity) {
  if (flood_opacity_ == flood_opacity)
    return false;
  flood_opacity_ = flood_opacity;
  return true;
}

sk_sp<PaintFilter> FEFlood::CreateImageFilter() {
  SkColor4f color = flood_color_;
  color.fA *= flood_opacity_;
  std::optional<PaintFilter::CropRect> crop_rect = GetCropRect();
  return sk_make_sp<ColorFilterPaintFilter>(
      cc::ColorFilter::MakeBlend(color, SkBlendMode::kSrc), nullptr,
      base::OptionalToPtr(crop_rect));
}

WTF::TextStream& FEFlood::ExternalRepresentation(WTF::TextStream& ts,
                                                 int indent) const {
  WriteIndent(ts, indent);
  ts << "[feFlood";
  FilterEffect::ExternalRepresentation(ts);
  // TODO(crbug.com/1308932): Color::NameForLayoutTreeAsText to SkColor4f
  ts << " flood-color=\""
     << Color::FromSkColor4f(FloodColor()).NameForLayoutTreeAsText() << "\" "
     << "flood-opacity=\"" << FloodOpacity() << "\"]\n";
  return ts;
}

}  // namespace blink
