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

#include "third_party/blink/renderer/platform/graphics/filters/fe_color_matrix.h"

#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"

namespace blink {

static const unsigned kColorMatrixSize = 20;

FEColorMatrix::FEColorMatrix(Filter* filter,
                             ColorMatrixType type,
                             const Vector<float>& values)
    : FilterEffect(filter), type_(type), values_(values) {}

ColorMatrixType FEColorMatrix::GetType() const {
  return type_;
}

bool FEColorMatrix::SetType(ColorMatrixType type) {
  if (type_ == type)
    return false;
  type_ = type;
  return true;
}

const Vector<float>& FEColorMatrix::Values() const {
  return values_;
}

bool FEColorMatrix::SetValues(const Vector<float>& values) {
  if (values_ == values)
    return false;
  values_ = values;
  return true;
}

static void SaturateMatrix(float s, float matrix[kColorMatrixSize]) {
  matrix[0] = 0.213f + 0.787f * s;
  matrix[1] = 0.715f - 0.715f * s;
  matrix[2] = 0.072f - 0.072f * s;
  matrix[3] = matrix[4] = 0;
  matrix[5] = 0.213f - 0.213f * s;
  matrix[6] = 0.715f + 0.285f * s;
  matrix[7] = 0.072f - 0.072f * s;
  matrix[8] = matrix[9] = 0;
  matrix[10] = 0.213f - 0.213f * s;
  matrix[11] = 0.715f - 0.715f * s;
  matrix[12] = 0.072f + 0.928f * s;
  matrix[13] = matrix[14] = 0;
  matrix[15] = matrix[16] = matrix[17] = 0;
  matrix[18] = 1;
  matrix[19] = 0;
}

static void HueRotateMatrix(float hue, float matrix[kColorMatrixSize]) {
  float cos_hue = cosf(hue * kPiFloat / 180);
  float sin_hue = sinf(hue * kPiFloat / 180);
  matrix[0] = 0.213f + cos_hue * 0.787f - sin_hue * 0.213f;
  matrix[1] = 0.715f - cos_hue * 0.715f - sin_hue * 0.715f;
  matrix[2] = 0.072f - cos_hue * 0.072f + sin_hue * 0.928f;
  matrix[3] = matrix[4] = 0;
  matrix[5] = 0.213f - cos_hue * 0.213f + sin_hue * 0.143f;
  matrix[6] = 0.715f + cos_hue * 0.285f + sin_hue * 0.140f;
  matrix[7] = 0.072f - cos_hue * 0.072f - sin_hue * 0.283f;
  matrix[8] = matrix[9] = 0;
  matrix[10] = 0.213f - cos_hue * 0.213f - sin_hue * 0.787f;
  matrix[11] = 0.715f - cos_hue * 0.715f + sin_hue * 0.715f;
  matrix[12] = 0.072f + cos_hue * 0.928f + sin_hue * 0.072f;
  matrix[13] = matrix[14] = 0;
  matrix[15] = matrix[16] = matrix[17] = 0;
  matrix[18] = 1;
  matrix[19] = 0;
}

static void LuminanceToAlphaMatrix(float matrix[kColorMatrixSize]) {
  memset(matrix, 0, kColorMatrixSize * sizeof(float));
  matrix[15] = 0.2125f;
  matrix[16] = 0.7154f;
  matrix[17] = 0.0721f;
}

static sk_sp<SkColorFilter> CreateColorFilter(ColorMatrixType type,
                                              const Vector<float>& values) {
  // Use defaults if values contains too few/many values.
  float matrix[kColorMatrixSize];
  memset(matrix, 0, kColorMatrixSize * sizeof(float));
  matrix[0] = matrix[6] = matrix[12] = matrix[18] = 1;

  switch (type) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
      break;
    case FECOLORMATRIX_TYPE_MATRIX:
      if (values.size() == kColorMatrixSize) {
        for (unsigned i = 0; i < kColorMatrixSize; ++i)
          matrix[i] = values[i];
      }
      break;
    case FECOLORMATRIX_TYPE_SATURATE:
      if (values.size() == 1)
        SaturateMatrix(values[0], matrix);
      break;
    case FECOLORMATRIX_TYPE_HUEROTATE:
      if (values.size() == 1)
        HueRotateMatrix(values[0], matrix);
      break;
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
      LuminanceToAlphaMatrix(matrix);
      break;
  }
  return SkColorFilters::Matrix(matrix);
}

bool FEColorMatrix::AffectsTransparentPixels() const {
  // Because the input pixels are premultiplied, the only way clear pixels can
  // be painted is if the additive component for the alpha is not 0.
  return type_ == FECOLORMATRIX_TYPE_MATRIX &&
         values_.size() >= kColorMatrixSize && values_[19] > 0;
}

sk_sp<PaintFilter> FEColorMatrix::CreateImageFilter() {
  sk_sp<PaintFilter> input(paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  sk_sp<SkColorFilter> filter = CreateColorFilter(type_, values_);
  PaintFilter::CropRect rect = GetCropRect();
  return sk_make_sp<ColorFilterPaintFilter>(std::move(filter), std::move(input),
                                            &rect);
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const ColorMatrixType& type) {
  switch (type) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
      ts << "UNKNOWN";
      break;
    case FECOLORMATRIX_TYPE_MATRIX:
      ts << "MATRIX";
      break;
    case FECOLORMATRIX_TYPE_SATURATE:
      ts << "SATURATE";
      break;
    case FECOLORMATRIX_TYPE_HUEROTATE:
      ts << "HUEROTATE";
      break;
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
      ts << "LUMINANCETOALPHA";
      break;
  }
  return ts;
}

static bool ValuesIsValidForType(ColorMatrixType type,
                                 const Vector<float>& values) {
  switch (type) {
    case FECOLORMATRIX_TYPE_MATRIX:
      return values.size() == kColorMatrixSize;
    case FECOLORMATRIX_TYPE_SATURATE:
    case FECOLORMATRIX_TYPE_HUEROTATE:
      return values.size() == 1;
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
      return values.size() == 0;
    case FECOLORMATRIX_TYPE_UNKNOWN:
      break;
  }
  NOTREACHED();
  return false;
}

WTF::TextStream& FEColorMatrix::ExternalRepresentation(WTF::TextStream& ts,
                                                       int indent) const {
  WriteIndent(ts, indent);
  ts << "[feColorMatrix";
  FilterEffect::ExternalRepresentation(ts);
  ts << " type=\"" << type_ << "\"";
  if (!values_.IsEmpty() && ValuesIsValidForType(type_, values_)) {
    ts << " values=\"";
    Vector<float>::const_iterator ptr = values_.begin();
    const Vector<float>::const_iterator end = values_.end();
    while (ptr < end) {
      ts << *ptr;
      ++ptr;
      if (ptr < end)
        ts << " ";
    }
    ts << "\"";
  }
  ts << "]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
