// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_INFO_H_

#include "base/types/strong_alias.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
namespace blink {

class ComputedStyle;
class Font;
class SimpleFontData;

enum class ResolvedUnderlinePosition {
  kNearAlphabeticBaselineAuto,
  kNearAlphabeticBaselineFromFont,
  kUnder,
  kOver
};

using MinimumThickness1 = base::StrongAlias<class MinimumThickness1Tag, bool>;

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
      const Font& scaled_font,
      const absl::optional<AppliedTextDecoration> selection_text_decoration,
      const ComputedStyle* decorating_box_style,
      MinimumThickness1 minimum_thickness1 = MinimumThickness1(true),
      float scaling_factor = 1.0f);

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
  void SetLineData(TextDecorationLine line, float line_offset);

  // These methods do not depend on SetDecorationIndex
  LayoutUnit Width() const { return width_; }
  float Baseline() const { return baseline_; }
  const ComputedStyle& Style() const { return style_; }
  float ComputedFontSize() const { return computed_font_size_; }
  const SimpleFontData* FontData() const { return font_data_; }
  // Returns the scaling factor for the decoration.
  // It can be different from NGFragmentItem::SvgScalingFactor() if the
  // text works as a resource.
  float ScalingFactor() const { return scaling_factor_; }
  ResolvedUnderlinePosition UnderlinePosition() const {
    return underline_position_;
  }
  bool ShouldAntialias() const { return antialias_; }
  float InkSkipClipUpper(float bounds_upper) const {
    return -baseline_ + bounds_upper - local_origin_.y();
  }

  // SetDecorationIndex must be called before using these methods.
  ETextDecorationStyle DecorationStyle() const;
  Color LineColor() const;
  float ResolvedThickness() const {
    return applied_decorations_thickness_[decoration_index_];
  }
  enum StrokeStyle StrokeStyle() const;

  // SetLineData must be called before using the remaining methods.
  gfx::PointF StartPoint() const;
  float DoubleOffset() const;

  // Compute bounds for the given line and the current decoration.
  gfx::RectF Bounds() const;

  // Return a path for current decoration.
  absl::optional<Path> StrokePath() const;

 private:
  float ComputeUnderlineThickness(
      const TextDecorationThickness& applied_decoration_thickness,
      const ComputedStyle* decorating_box_style);

  gfx::RectF BoundsForDottedOrDashed() const;
  gfx::RectF BoundsForWavy() const;
  float WavyDecorationSizing() const;
  float ControlPointDistanceFromResolvedThickness() const;
  float StepFromResolvedThickness() const;
  Path PrepareDottedOrDashedStrokePath() const;
  Path PrepareWavyStrokePath() const;
  bool IsSpellingOrGrammarError() const {
    return line_data_.line == TextDecorationLine::kSpellingError ||
           line_data_.line == TextDecorationLine::kGrammarError;
  }

  const ComputedStyle& style_;
  const absl::optional<AppliedTextDecoration> selection_text_decoration_;
  const FontBaseline baseline_type_;
  const LayoutUnit width_;
  const SimpleFontData* font_data_;
  const float baseline_;
  const float computed_font_size_;
  const float scaling_factor_;
  ResolvedUnderlinePosition underline_position_;
  gfx::PointF local_origin_;
  const bool minimum_thickness_is_one_;
  bool antialias_;
  Vector<float> applied_decorations_thickness_;
  int decoration_index_;

  struct LineData {
    TextDecorationLine line;
    float line_offset;
    float double_offset;
    int wavy_offset_factor;
    absl::optional<Path> stroke_path;
  };
  LineData line_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_INFO_H_
