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
#include "base/numerics/safe_conversions.h"
#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

FEConvolveMatrix::FEConvolveMatrix(Filter* filter,
                                   const gfx::Size& kernel_size,
                                   float divisor,
                                   float bias,
                                   const gfx::Vector2d& target_offset,
                                   FEConvolveMatrix::EdgeModeType edge_mode,
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

gfx::RectF FEConvolveMatrix::MapEffect(const gfx::RectF& rect) const {
  if (!ParametersValid())
    return rect;
  gfx::RectF result = rect;
  result.Offset(gfx::Vector2dF(-target_offset_));
  result.set_size(result.size() + gfx::SizeF(kernel_size_));
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

bool FEConvolveMatrix::SetTargetOffset(const gfx::Vector2d& target_offset) {
  if (target_offset_ == target_offset)
    return false;
  target_offset_ = target_offset;
  return true;
}

bool FEConvolveMatrix::SetEdgeMode(FEConvolveMatrix::EdgeModeType edge_mode) {
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

static SkTileMode ToSkiaTileMode(FEConvolveMatrix::EdgeModeType edge_mode) {
  switch (edge_mode) {
    case FEConvolveMatrix::EDGEMODE_DUPLICATE:
      return SkTileMode::kClamp;
    case FEConvolveMatrix::EDGEMODE_WRAP:
      return SkTileMode::kRepeat;
    case FEConvolveMatrix::EDGEMODE_NONE:
      return SkTileMode::kDecal;
    default:
      return SkTileMode::kClamp;
  }
}

bool FEConvolveMatrix::ParametersValid() const {
  if (kernel_size_.IsEmpty())
    return false;
  uint64_t kernel_area = kernel_size_.Area64();
  if (!base::CheckedNumeric<int>(kernel_area).IsValid())
    return false;
  if (base::checked_cast<size_t>(kernel_area) != kernel_matrix_.size())
    return false;
  if (target_offset_.x() < 0 || target_offset_.x() >= kernel_size_.width())
    return false;
  if (target_offset_.y() < 0 || target_offset_.y() >= kernel_size_.height())
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
      SkISize::Make(kernel_size_.width(), kernel_size_.height()));
  // parametersValid() above checks that the kernel area fits in int.
  int num_elements = base::checked_cast<int>(kernel_size_.Area64());
  SkScalar gain = SkFloatToScalar(1.0f / divisor_);
  SkScalar bias = SkFloatToScalar(bias_ * 255);
  SkIPoint target = SkIPoint::Make(target_offset_.x(), target_offset_.y());
  SkTileMode tile_mode = ToSkiaTileMode(edge_mode_);
  bool convolve_alpha = !preserve_alpha_;
  auto kernel = std::make_unique<SkScalar[]>(num_elements);
  for (int i = 0; i < num_elements; ++i)
    kernel[i] = SkFloatToScalar(kernel_matrix_[num_elements - 1 - i]);
  std::optional<PaintFilter::CropRect> crop_rect = GetCropRect();
  return sk_make_sp<MatrixConvolutionPaintFilter>(
      kernel_size, kernel.get(), gain, bias, target, tile_mode, convolve_alpha,
      std::move(input), base::OptionalToPtr(crop_rect));
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const FEConvolveMatrix::EdgeModeType& type) {
  switch (type) {
    case FEConvolveMatrix::EDGEMODE_UNKNOWN:
      ts << "UNKNOWN";
      break;
    case FEConvolveMatrix::EDGEMODE_DUPLICATE:
      ts << "DUPLICATE";
      break;
    case FEConvolveMatrix::EDGEMODE_WRAP:
      ts << "WRAP";
      break;
    case FEConvolveMatrix::EDGEMODE_NONE:
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
  ts << " order=\"" << kernel_size_.ToString() << "\" "
     << "kernelMatrix=\"" << kernel_matrix_ << "\" "
     << "divisor=\"" << divisor_ << "\" "
     << "bias=\"" << bias_ << "\" "
     << "target=\"" << target_offset_.ToString() << "\" "
     << "edgeMode=\"" << edge_mode_ << "\" "
     << "preserveAlpha=\"" << preserve_alpha_ << "\"]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
