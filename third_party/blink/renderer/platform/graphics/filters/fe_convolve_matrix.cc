/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
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

#include "third_party/blink/renderer/platform/graphics/filters/fe_convolve_matrix.h"

#include <memory>

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/skia/include/effects/SkMatrixConvolutionImageFilter.h"

namespace blink {

FEConvolveMatrix::FEConvolveMatrix(Filter* filter,
                                   const IntSize& kernel_size,
                                   float divisor,
                                   float bias,
                                   const IntPoint& target_offset,
                                   EdgeModeType edge_mode,
                                   bool preserve_alpha,
                                   const Vector<float>& kernel_matrix)
    : FilterEffect(filter),
      kernel_size_(kernel_size),
      divisor_(divisor),
      bias_(bias),
      target_offset_(target_offset),
      edge_mode_(edge_mode),
      preserve_alpha_(preserve_alpha),
      kernel_matrix_(kernel_matrix) {}

FloatRect FEConvolveMatrix::MapEffect(const FloatRect& rect) const {
  if (!ParametersValid())
    return rect;
  FloatRect result = rect;
  result.MoveBy(FloatPoint(-target_offset_));
  result.Expand(FloatSize(kernel_size_));
  return result;
}

bool FEConvolveMatrix::SetDivisor(float divisor) {
  if (divisor_ == divisor)
    return false;
  divisor_ = divisor;
  return true;
}

bool FEConvolveMatrix::SetBias(float bias) {
  if (bias_ == bias)
    return false;
  bias_ = bias;
  return true;
}

bool FEConvolveMatrix::SetTargetOffset(const IntPoint& target_offset) {
  if (target_offset_ == target_offset)
    return false;
  target_offset_ = target_offset;
  return true;
}

bool FEConvolveMatrix::SetEdgeMode(EdgeModeType edge_mode) {
  if (edge_mode_ == edge_mode)
    return false;
  edge_mode_ = edge_mode;
  return true;
}

bool FEConvolveMatrix::SetPreserveAlpha(bool preserve_alpha) {
  if (preserve_alpha_ == preserve_alpha)
    return false;
  preserve_alpha_ = preserve_alpha;
  return true;
}

SkMatrixConvolutionImageFilter::TileMode ToSkiaTileMode(
    EdgeModeType edge_mode) {
  switch (edge_mode) {
    case EDGEMODE_DUPLICATE:
      return SkMatrixConvolutionImageFilter::kClamp_TileMode;
    case EDGEMODE_WRAP:
      return SkMatrixConvolutionImageFilter::kRepeat_TileMode;
    case EDGEMODE_NONE:
      return SkMatrixConvolutionImageFilter::kClampToBlack_TileMode;
    default:
      return SkMatrixConvolutionImageFilter::kClamp_TileMode;
  }
}

bool FEConvolveMatrix::ParametersValid() const {
  if (kernel_size_.IsEmpty())
    return false;
  uint64_t kernel_area = kernel_size_.Area();
  if (!base::CheckedNumeric<int>(kernel_area).IsValid())
    return false;
  if (SafeCast<size_t>(kernel_area) != kernel_matrix_.size())
    return false;
  if (target_offset_.X() < 0 || target_offset_.X() >= kernel_size_.Width())
    return false;
  if (target_offset_.Y() < 0 || target_offset_.Y() >= kernel_size_.Height())
    return false;
  if (!divisor_)
    return false;
  return true;
}

sk_sp<PaintFilter> FEConvolveMatrix::CreateImageFilter() {
  if (!ParametersValid())
    return CreateTransparentBlack();

  sk_sp<PaintFilter> input(paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  SkISize kernel_size(
      SkISize::Make(kernel_size_.Width(), kernel_size_.Height()));
  // parametersValid() above checks that the kernel area fits in int.
  int num_elements = SafeCast<int>(kernel_size_.Area());
  SkScalar gain = SkFloatToScalar(1.0f / divisor_);
  SkScalar bias = SkFloatToScalar(bias_ * 255);
  SkIPoint target = SkIPoint::Make(target_offset_.X(), target_offset_.Y());
  MatrixConvolutionPaintFilter::TileMode tile_mode = ToSkiaTileMode(edge_mode_);
  bool convolve_alpha = !preserve_alpha_;
  auto kernel = std::make_unique<SkScalar[]>(num_elements);
  for (int i = 0; i < num_elements; ++i)
    kernel[i] = SkFloatToScalar(kernel_matrix_[num_elements - 1 - i]);
  PaintFilter::CropRect crop_rect = GetCropRect();
  return sk_make_sp<MatrixConvolutionPaintFilter>(
      kernel_size, kernel.get(), gain, bias, target, tile_mode, convolve_alpha,
      std::move(input), &crop_rect);
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const EdgeModeType& type) {
  switch (type) {
    case EDGEMODE_UNKNOWN:
      ts << "UNKNOWN";
      break;
    case EDGEMODE_DUPLICATE:
      ts << "DUPLICATE";
      break;
    case EDGEMODE_WRAP:
      ts << "WRAP";
      break;
    case EDGEMODE_NONE:
      ts << "NONE";
      break;
  }
  return ts;
}

WTF::TextStream& FEConvolveMatrix::ExternalRepresentation(WTF::TextStream& ts,
                                                          int indent) const {
  WriteIndent(ts, indent);
  ts << "[feConvolveMatrix";
  FilterEffect::ExternalRepresentation(ts);
  ts << " order=\"" << FloatSize(kernel_size_) << "\" "
     << "kernelMatrix=\"" << kernel_matrix_ << "\" "
     << "divisor=\"" << divisor_ << "\" "
     << "bias=\"" << bias_ << "\" "
     << "target=\"" << target_offset_ << "\" "
     << "edgeMode=\"" << edge_mode_ << "\" "
     << "preserveAlpha=\"" << preserve_alpha_ << "\"]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
