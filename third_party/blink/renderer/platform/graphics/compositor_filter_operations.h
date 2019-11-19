// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITOR_FILTER_OPERATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITOR_FILTER_OPERATIONS_H_

#include "cc/paint/filter_operations.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {

// An ordered list of filter operations.
class PLATFORM_EXPORT CompositorFilterOperations {
 public:
  const cc::FilterOperations& AsCcFilterOperations() const;
  cc::FilterOperations ReleaseCcFilterOperations();

  void AppendGrayscaleFilter(float amount);
  void AppendSepiaFilter(float amount);
  void AppendSaturateFilter(float amount);
  void AppendHueRotateFilter(float amount);
  void AppendInvertFilter(float amount);
  void AppendBrightnessFilter(float amount);
  void AppendContrastFilter(float amount);
  void AppendOpacityFilter(float amount);
  void AppendBlurFilter(float amount,
                        SkBlurImageFilter::TileMode tile_mode =
                            SkBlurImageFilter::kClampToBlack_TileMode);
  void AppendDropShadowFilter(IntPoint offset, float std_deviation, Color);
  void AppendColorMatrixFilter(const cc::FilterOperation::Matrix&);
  void AppendZoomFilter(float amount, int inset);
  void AppendSaturatingBrightnessFilter(float amount);

  void AppendReferenceFilter(sk_sp<PaintFilter>);

  void Clear();
  bool IsEmpty() const;
  size_t size() const { return filter_operations_.size(); }

  // Returns a rect covering the destination pixels that can be affected by
  // source pixels in |inputRect|.
  FloatRect MapRect(const FloatRect& input_rect) const;

  bool HasFilterThatMovesPixels() const;

  void SetReferenceBox(const FloatRect& r) { reference_box_ = r; }
  FloatRect ReferenceBox() const { return reference_box_; }

  // For reference filters, this equality operator compares pointers of the
  // image_filter fields instead of their values.
  bool operator==(const CompositorFilterOperations&) const;
  bool operator!=(const CompositorFilterOperations& o) const {
    return !(*this == o);
  }

  String ToString() const;

 private:
  cc::FilterOperations filter_operations_;
  FloatRect reference_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITOR_FILTER_OPERATIONS_H_
