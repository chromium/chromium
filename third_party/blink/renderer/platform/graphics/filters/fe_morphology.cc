/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/platform/graphics/filters/fe_morphology.h"

#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/skia/include/effects/SkMorphologyImageFilter.h"

namespace blink {

FEMorphology::FEMorphology(Filter* filter,
                           MorphologyOperatorType type,
                           float radius_x,
                           float radius_y)
    : FilterEffect(filter),
      type_(type),
      radius_x_(std::max(0.0f, radius_x)),
      radius_y_(std::max(0.0f, radius_y)) {}

MorphologyOperatorType FEMorphology::MorphologyOperator() const {
  return type_;
}

bool FEMorphology::SetMorphologyOperator(MorphologyOperatorType type) {
  if (type_ == type)
    return false;
  type_ = type;
  return true;
}

float FEMorphology::RadiusX() const {
  return radius_x_;
}

bool FEMorphology::SetRadiusX(float radius_x) {
  radius_x = std::max(0.0f, radius_x);
  if (radius_x_ == radius_x)
    return false;
  radius_x_ = radius_x;
  return true;
}

float FEMorphology::RadiusY() const {
  return radius_y_;
}

bool FEMorphology::SetRadiusY(float radius_y) {
  radius_y = std::max(0.0f, radius_y);
  if (radius_y_ == radius_y)
    return false;
  radius_y_ = radius_y;
  return true;
}

FloatRect FEMorphology::MapEffect(const FloatRect& rect) const {
  FloatRect result = rect;
  result.InflateX(GetFilter()->ApplyHorizontalScale(radius_x_));
  result.InflateY(GetFilter()->ApplyVerticalScale(radius_y_));
  return result;
}

sk_sp<PaintFilter> FEMorphology::CreateImageFilter() {
  sk_sp<PaintFilter> input(paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  int radius_x = clampTo<int>(GetFilter()->ApplyHorizontalScale(radius_x_));
  int radius_y = clampTo<int>(GetFilter()->ApplyVerticalScale(radius_y_));
  PaintFilter::CropRect rect = GetCropRect();
  MorphologyPaintFilter::MorphType morph_type =
      type_ == FEMORPHOLOGY_OPERATOR_DILATE
          ? MorphologyPaintFilter::MorphType::kDilate
          : MorphologyPaintFilter::MorphType::kErode;
  return sk_make_sp<MorphologyPaintFilter>(morph_type, radius_x, radius_y,
                                           std::move(input), &rect);
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const MorphologyOperatorType& type) {
  switch (type) {
    case FEMORPHOLOGY_OPERATOR_UNKNOWN:
      ts << "UNKNOWN";
      break;
    case FEMORPHOLOGY_OPERATOR_ERODE:
      ts << "ERODE";
      break;
    case FEMORPHOLOGY_OPERATOR_DILATE:
      ts << "DILATE";
      break;
  }
  return ts;
}

WTF::TextStream& FEMorphology::ExternalRepresentation(WTF::TextStream& ts,
                                                      int indent) const {
  WriteIndent(ts, indent);
  ts << "[feMorphology";
  FilterEffect::ExternalRepresentation(ts);
  ts << " operator=\"" << MorphologyOperator() << "\" "
     << "radius=\"" << RadiusX() << ", " << RadiusY() << "\"]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
