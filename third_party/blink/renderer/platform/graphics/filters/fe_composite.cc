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

#include "third_party/blink/renderer/platform/graphics/filters/fe_composite.h"

#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/skia/include/effects/SkArithmeticImageFilter.h"
#include "third_party/skia/include/effects/SkXfermodeImageFilter.h"

namespace blink {

FEComposite::FEComposite(Filter* filter,
                         const CompositeOperationType& type,
                         float k1,
                         float k2,
                         float k3,
                         float k4)
    : FilterEffect(filter), type_(type), k1_(k1), k2_(k2), k3_(k3), k4_(k4) {}

CompositeOperationType FEComposite::Operation() const {
  return type_;
}

bool FEComposite::SetOperation(CompositeOperationType type) {
  if (type_ == type)
    return false;
  type_ = type;
  return true;
}

float FEComposite::K1() const {
  return k1_;
}

bool FEComposite::SetK1(float k1) {
  if (k1_ == k1)
    return false;
  k1_ = k1;
  return true;
}

float FEComposite::K2() const {
  return k2_;
}

bool FEComposite::SetK2(float k2) {
  if (k2_ == k2)
    return false;
  k2_ = k2;
  return true;
}

float FEComposite::K3() const {
  return k3_;
}

bool FEComposite::SetK3(float k3) {
  if (k3_ == k3)
    return false;
  k3_ = k3;
  return true;
}

float FEComposite::K4() const {
  return k4_;
}

bool FEComposite::SetK4(float k4) {
  if (k4_ == k4)
    return false;
  k4_ = k4;
  return true;
}

bool FEComposite::AffectsTransparentPixels() const {
  // When k4 is non-zero (greater than zero with clamping factored in), the
  // arithmetic operation will produce non-transparent output for transparent
  // output.
  return type_ == FECOMPOSITE_OPERATOR_ARITHMETIC && K4() > 0;
}

FloatRect FEComposite::MapInputs(const FloatRect& rect) const {
  FloatRect i1 = InputEffect(0)->MapRect(rect);
  FloatRect i2 = InputEffect(1)->MapRect(rect);
  switch (type_) {
    case FECOMPOSITE_OPERATOR_IN:
      // 'in' has output only in the intersection of both inputs.
      return Intersection(i1, i2);
    case FECOMPOSITE_OPERATOR_ATOP:
      // 'atop' has output only in the extents of the second input.
      return i2;
    case FECOMPOSITE_OPERATOR_ARITHMETIC:
      // result(i1,i2) = k1*i1*i2 + k2*i1 + k3*i2 + k4
      //
      // (The below is not a complete breakdown of cases.)
      //
      // Arithmetic with positive k4 may influence the complete filter primitive
      // region. [k4 > 0 => result(0,0) = k4 => result(i1,i2) >= k4]
      // Fall through to use union. If this effect clips to bounds,
      // ApplyBounds() will return AbsoluteBounds() regardless of the return
      // value of this function because AffectsTransparentPixels() is true.
      if (K4() > 0)
        break;
      // If both K2 or K3 are positive, both i1 and i2 appear. Fall through to
      // use union.
      if (K2() > 0 && K3() > 0)
        break;
      // If k2 > 0, output can be produced whenever i1 is non-transparent.
      // [k3 = k4 = 0 => result(i1,i2) = k1*i1*i2 + k2*i1 = (k1*i2 + k2)*i1]
      if (K2() > 0)
        return i1;
      // If k3 > 0, output can be produced whenever i2 is non-transparent.
      // [k2 = k4 = 0 => result(i1,i2) = k1*i1*i2 + k3*i2 = (k1*i1 + k3)*i2]
      if (K3() > 0)
        return i2;
      // If just k1 is positive, output will only be produce where both inputs
      // are non-transparent. Use intersection.
      // [k1 > 0 and k2 = k3 = k4 = 0 => result(i1,i2) = k1*i1*i2]
      if (K1() > 0)
        return Intersection(i1, i2);
      // [k1 = k2 = k3 = k4 = 0 => result(i1,i2) = 0]
      return FloatRect();
    default:
      break;
  }
  // Take the union of both input effects.
  return UnionRect(i1, i2);
}

SkBlendMode ToBlendMode(CompositeOperationType mode) {
  switch (mode) {
    case FECOMPOSITE_OPERATOR_OVER:
      return SkBlendMode::kSrcOver;
    case FECOMPOSITE_OPERATOR_IN:
      return SkBlendMode::kSrcIn;
    case FECOMPOSITE_OPERATOR_OUT:
      return SkBlendMode::kSrcOut;
    case FECOMPOSITE_OPERATOR_ATOP:
      return SkBlendMode::kSrcATop;
    case FECOMPOSITE_OPERATOR_XOR:
      return SkBlendMode::kXor;
    case FECOMPOSITE_OPERATOR_LIGHTER:
      return SkBlendMode::kPlus;
    default:
      NOTREACHED();
      return SkBlendMode::kSrcOver;
  }
}

sk_sp<PaintFilter> FEComposite::CreateImageFilter() {
  return CreateImageFilterInternal(true);
}

sk_sp<PaintFilter> FEComposite::CreateImageFilterWithoutValidation() {
  return CreateImageFilterInternal(false);
}

sk_sp<PaintFilter> FEComposite::CreateImageFilterInternal(
    bool requires_pm_color_validation) {
  sk_sp<PaintFilter> foreground(
      paint_filter_builder::Build(InputEffect(0), OperatingInterpolationSpace(),
                                  !MayProduceInvalidPreMultipliedPixels()));
  sk_sp<PaintFilter> background(
      paint_filter_builder::Build(InputEffect(1), OperatingInterpolationSpace(),
                                  !MayProduceInvalidPreMultipliedPixels()));
  PaintFilter::CropRect crop_rect = GetCropRect();

  if (type_ == FECOMPOSITE_OPERATOR_ARITHMETIC) {
    return sk_make_sp<ArithmeticPaintFilter>(
        SkFloatToScalar(k1_), SkFloatToScalar(k2_), SkFloatToScalar(k3_),
        SkFloatToScalar(k4_), requires_pm_color_validation,
        std::move(background), std::move(foreground), &crop_rect);
  }

  return sk_make_sp<XfermodePaintFilter>(ToBlendMode(type_),
                                         std::move(background),
                                         std::move(foreground), &crop_rect);
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const CompositeOperationType& type) {
  switch (type) {
    case FECOMPOSITE_OPERATOR_UNKNOWN:
      ts << "UNKNOWN";
      break;
    case FECOMPOSITE_OPERATOR_OVER:
      ts << "OVER";
      break;
    case FECOMPOSITE_OPERATOR_IN:
      ts << "IN";
      break;
    case FECOMPOSITE_OPERATOR_OUT:
      ts << "OUT";
      break;
    case FECOMPOSITE_OPERATOR_ATOP:
      ts << "ATOP";
      break;
    case FECOMPOSITE_OPERATOR_XOR:
      ts << "XOR";
      break;
    case FECOMPOSITE_OPERATOR_ARITHMETIC:
      ts << "ARITHMETIC";
      break;
    case FECOMPOSITE_OPERATOR_LIGHTER:
      ts << "LIGHTER";
      break;
  }
  return ts;
}

WTF::TextStream& FEComposite::ExternalRepresentation(WTF::TextStream& ts,
                                                     int indent) const {
  WriteIndent(ts, indent);
  ts << "[feComposite";
  FilterEffect::ExternalRepresentation(ts);
  ts << " operation=\"" << type_ << "\"";
  if (type_ == FECOMPOSITE_OPERATOR_ARITHMETIC)
    ts << " k1=\"" << k1_ << "\" k2=\"" << k2_ << "\" k3=\"" << k3_
       << "\" k4=\"" << k4_ << "\"";
  ts << "]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  InputEffect(1)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
