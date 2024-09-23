// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"

#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

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

void CompositorFilterOperations::AppendColorMatrixFilter(Vector<float> values) {
  DCHECK_EQ(values.size(), 20u);
  cc::FilterOperation::Matrix matrix = {};
  for (WTF::wtf_size_t i = 0; i < values.size(); ++i)
    matrix[i] = values[i];
  filter_operations_.Append(
      cc::FilterOperation::CreateColorMatrixFilter(matrix));
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

void CompositorFilterOperations::AppendBlurFilter(float amount,
                                                  SkTileMode tile_mode) {
  filter_operations_.Append(
      cc::FilterOperation::CreateBlurFilter(amount, tile_mode));
}

void CompositorFilterOperations::AppendDropShadowFilter(gfx::Vector2d offset,
                                                        float std_deviation,
                                                        const Color& color) {
  gfx::Point gfx_offset(offset.x(), offset.y());
  // TODO(crbug/1308932): Remove FromColor and make all SkColor4f.
  filter_operations_.Append(cc::FilterOperation::CreateDropShadowFilter(
      gfx_offset, std_deviation, SkColor4f::FromColor(color.Rgb())));
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

gfx::RectF CompositorFilterOperations::MapRect(
    const gfx::RectF& input_rect) const {
  return gfx::RectF(
      filter_operations_.MapRect(gfx::ToEnclosingRect(input_rect)));
}

bool CompositorFilterOperations::HasFilterThatMovesPixels() const {
  return filter_operations_.HasFilterThatMovesPixels();
}

bool CompositorFilterOperations::HasReferenceFilter() const {
  return filter_operations_.HasReferenceFilter();
}

bool CompositorFilterOperations::operator==(
    const CompositorFilterOperations& o) const {
  return reference_box_ == o.reference_box_ &&
         filter_operations_ == o.filter_operations_;
}

String CompositorFilterOperations::ToString() const {
  return String(filter_operations_.ToString()) + " at " +
         String(reference_box_.ToString());
}

}  // namespace blink
