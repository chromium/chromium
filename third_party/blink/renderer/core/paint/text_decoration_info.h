// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_INFO_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
namespace blink {

class ComputedStyle;
class SimpleFontData;

enum class ResolvedUnderlinePosition {
  kNearAlphabeticBaselineAuto,
  kNearAlphabeticBaselineFromFont,
  kUnder,
  kOver
};

// Container for computing and storing information for text decoration
// invalidation and painting. See also
// https://www.w3.org/TR/css-text-decor-3/#painting-order
class CORE_EXPORT TextDecorationInfo {
  STACK_ALLOCATED();

 public:
  TextDecorationInfo(
      PhysicalOffset local_origin,
      LayoutUnit width,
      FontBaseline baseline_type,
      const ComputedStyle& style,
      const absl::optional<AppliedTextDecoration> selection_text_decoration,
      const ComputedStyle* decorating_box_style);

  // Set the decoration to use when painting and returning values.
  // Must be set before calling any other method, and can be called
  // again at any time. This object will use the most recently given
  // index for any computation that uses data from an
  // AppliedTextDecoration object. The index must be a valid index
  // the AppliedTextDecorations contained within the style passed
  // at construction.
  void SetDecorationIndex(int decoration_index);

  // Set data for one of the text decoration lines: over, under or
  // through. Must be called before trying to paint or compute bounds
  // for a line.
  void SetPerLineData(TextDecoration line,
                      float line_offset,
                      float double_offset,
                      int wavy_offset_factor);

  // These methods do not depend on SetDecorationIndex
  LayoutUnit Width() const { return width_; }
  float Baseline() const { return baseline_; }
  const ComputedStyle& Style() const { return style_; }
  const SimpleFontData* FontData() const { return font_data_; }
  ResolvedUnderlinePosition UnderlinePosition() const {
    return underline_position_;
  }
  bool ShouldAntialias() const { return antialias_; }
  float InkSkipClipUpper(float bounds_upper) const {
    return -baseline_ + bounds_upper - local_origin_.Y();
  }

  // SetDecorationIndex must be called before using these methods.
  ETextDecorationStyle DecorationStyle() const;
  Color LineColor() const;
  float ResolvedThickness() const {
    return applied_decorations_thickness_[decoration_index_];
  }
  enum StrokeStyle StrokeStyle() const;

  // SetPerLineData must be called with the line argument before using
  // the remaining methods.
  FloatPoint StartPoint(TextDecoration line) const;
  float DoubleOffset(TextDecoration line) const;

  // Compute bounds for the given line and the current decoration.
  FloatRect BoundsForLine(TextDecoration line) const;

  // Return a path for a wavy line at the given position, for the
  // current decoration.
  absl::optional<Path> PrepareWavyStrokePath(TextDecoration line) const;

  static float DoubleOffsetFromThickness(float thickness_pixels) {
    return thickness_pixels + 1.0f;
  }

 private:
  float ComputeUnderlineThickness(
      const TextDecorationThickness& applied_decoration_thickness,
      const ComputedStyle* decorating_box_style);

  FloatRect BoundsForDottedOrDashed(TextDecoration line) const;
  FloatRect BoundsForWavy(TextDecoration line) const;

  const ComputedStyle& style_;
  const absl::optional<AppliedTextDecoration> selection_text_decoration_;
  const FontBaseline baseline_type_;
  const LayoutUnit width_;
  const SimpleFontData* font_data_;
  const float baseline_;
  ResolvedUnderlinePosition underline_position_;
  FloatPoint local_origin_;
  bool antialias_;
  Vector<float> applied_decorations_thickness_;

  int decoration_index_;

  /* We need to store data for up to 3 lines: Underline, Overline and
     LineThrough. Unfortunately the enum for these are bitfield indices, not
     directly useful as indexes. So explicitly convert in place
     when necessary.
  */
  struct PerLineData {
    float line_offset;
    float double_offset;
    int wavy_offset_factor;
    mutable absl::optional<Path> stroke_path;
  };
  PerLineData line_data_[3];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_INFO_H_
