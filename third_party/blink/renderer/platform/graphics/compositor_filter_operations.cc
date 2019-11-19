// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"

#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

const cc::FilterOperations& CompositorFilterOperations::AsCcFilterOperations()
    const {
  return filter_operations_;
}

cc::FilterOperations CompositorFilterOperations::ReleaseCcFilterOperations() {
  return std::move(filter_operations_);
}

void CompositorFilterOperations::AppendGrayscaleFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateGrayscaleFilter(amount));
}

void CompositorFilterOperations::AppendSepiaFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateSepiaFilter(amount));
}

void CompositorFilterOperations::AppendSaturateFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateSaturateFilter(amount));
}

void CompositorFilterOperations::AppendHueRotateFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateHueRotateFilter(amount));
}

void CompositorFilterOperations::AppendInvertFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateInvertFilter(amount));
}

void CompositorFilterOperations::AppendBrightnessFilter(float amount) {
  filter_operations_.Append(
      cc::FilterOperation::CreateBrightnessFilter(amount));
}

void CompositorFilterOperations::AppendContrastFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateContrastFilter(amount));
}

void CompositorFilterOperations::AppendOpacityFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateOpacityFilter(amount));
}

void CompositorFilterOperations::AppendBlurFilter(
    float amount,
    SkBlurImageFilter::TileMode tile_mode) {
  filter_operations_.Append(
      cc::FilterOperation::CreateBlurFilter(amount, tile_mode));
}

void CompositorFilterOperations::AppendDropShadowFilter(IntPoint offset,
                                                        float std_deviation,
                                                        Color color) {
  gfx::Point gfx_offset(offset.X(), offset.Y());
  filter_operations_.Append(cc::FilterOperation::CreateDropShadowFilter(
      gfx_offset, std_deviation, color.Rgb()));
}

void CompositorFilterOperations::AppendColorMatrixFilter(
    const cc::FilterOperation::Matrix& matrix) {
  filter_operations_.Append(
      cc::FilterOperation::CreateColorMatrixFilter(matrix));
}

void CompositorFilterOperations::AppendZoomFilter(float amount, int inset) {
  filter_operations_.Append(
      cc::FilterOperation::CreateZoomFilter(amount, inset));
}

void CompositorFilterOperations::AppendSaturatingBrightnessFilter(
    float amount) {
  filter_operations_.Append(
      cc::FilterOperation::CreateSaturatingBrightnessFilter(amount));
}

void CompositorFilterOperations::AppendReferenceFilter(
    sk_sp<PaintFilter> image_filter) {
  filter_operations_.Append(
      cc::FilterOperation::CreateReferenceFilter(std::move(image_filter)));
}

void CompositorFilterOperations::Clear() {
  filter_operations_.Clear();
}

bool CompositorFilterOperations::IsEmpty() const {
  return filter_operations_.IsEmpty();
}

FloatRect CompositorFilterOperations::MapRect(
    const FloatRect& input_rect) const {
  gfx::Rect result =
      filter_operations_.MapRect(EnclosingIntRect(input_rect), SkMatrix::I());
  return FloatRect(result.x(), result.y(), result.width(), result.height());
}

bool CompositorFilterOperations::HasFilterThatMovesPixels() const {
  return filter_operations_.HasFilterThatMovesPixels();
}

bool CompositorFilterOperations::operator==(
    const CompositorFilterOperations& o) const {
  return reference_box_ == o.reference_box_ &&
         filter_operations_ == o.filter_operations_;
}

String CompositorFilterOperations::ToString() const {
  return String(filter_operations_.ToString().c_str()) + " at " +
         reference_box_.ToString();
}

}  // namespace blink
