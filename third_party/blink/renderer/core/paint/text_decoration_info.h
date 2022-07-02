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
class NGDecoratingBox;
class NGInlinePaintContext;
class SimpleFontData;
class TextDecorationOffsetBase;

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
      const ComputedStyle& target_style,
      const NGInlinePaintContext* inline_context,
      const absl::optional<AppliedTextDecoration> selection_text_decoration,
      const Font* font_override = nullptr,
      MinimumThickness1 minimum_thickness1 = MinimumThickness1(true),
      float scaling_factor = 1.0f,
      // Following arguments are used only in legacy. They're deprecated.
      absl::optional<FontBaseline> baseline_type_override = absl::nullopt,
      const ComputedStyle* decorating_box_style = nullptr);

  const AppliedTextDecoration& GetAppliedTextDecoration() const {
    DCHECK(applied_text_decoration_);
    return *applied_text_decoration_;
  }
  bool HasUnderline() const { return has_underline_; }
  bool HasOverline() const { return has_overline_; }
  bool HasLineThrough() const { return Has(TextDecorationLine::kLineThrough); }
  bool HasSpellingError() const {
    return Has(TextDecorationLine::kSpellingError);
  }
  bool HasGrammarError() const {
    return Has(TextDecorationLine::kGrammarError);
  }
  bool HasSpellingOrGrammerError() const {
    return HasSpellingError() || HasGrammarError();
  }

  // Set the decoration to use when painting and returning values.
  //
  // This is set to 0 when constructed, and can be called again at any time.
  // This object will use the most recently given index for any computation that
  // uses data from an AppliedTextDecoration object or a decorating box.
  //
  // The index must be a valid index the AppliedTextDecorations contained within
  // the style passed at construction.
  void SetDecorationIndex(int decoration_index);

  // Set data for one of the text decoration lines: over, under or
  // through. Must be called before trying to paint or compute bounds
  // for a line.
  void SetLineData(TextDecorationLine line, float line_offset);
  void SetUnderlineLineData(const TextDecorationOffsetBase& decoration_offset);
  void SetOverlineLineData(const TextDecorationOffsetBase& decoration_offset);
  void SetLineThroughLineData();
  void SetSpellingOrGrammarErrorLineData(const TextDecorationOffsetBase&);

  // These methods do not depend on |SetDecorationIndex|.
  LayoutUnit Width() const { return width_; }
  const ComputedStyle& TargetStyle() const { return target_style_; }
  float TargetAscent() const { return target_ascent_; }
  // Returns the scaling factor for the decoration.
  // It can be different from NGFragmentItem::SvgScalingFactor() if the
  // text works as a resource.
  float ScalingFactor() const { return scaling_factor_; }
  bool ShouldAntialias() const { return antialias_; }
  float InkSkipClipUpper(float bounds_upper) const {
    return -TargetAscent() + bounds_upper - local_origin_.top.ToFloat();
  }

  // |SetDecorationIndex| may change the results of these methods.
  float ComputedFontSize() const { return computed_font_size_; }
  const SimpleFontData* FontData() const { return font_data_; }
  float Ascent() const { return ascent_; }
  ETextDecorationStyle DecorationStyle() const;
  ResolvedUnderlinePosition FlippedUnderlinePosition() const {
    return flipped_underline_position_;
  }
  ResolvedUnderlinePosition OriginalUnderlinePosition() const {
    return original_underline_position_;
  }
  Color LineColor() const;
  float ResolvedThickness() const { return resolved_thickness_; }
  enum StrokeStyle StrokeStyle() const;

  // SetLineData must be called before using the remaining methods.
  gfx::PointF StartPoint() const;
  float DoubleOffset() const;

  // Compute bounds for the given line and the current decoration.
  gfx::RectF Bounds() const;

  // Return a path for current decoration.
  absl::optional<Path> StrokePath() const;

  // Overrides the line color with the given topmost active highlight ‘color’.
  void SetHighlightOverrideColor(const absl::optional<Color>&);

 private:
  bool Has(TextDecorationLine line) const { return EnumHasFlags(lines_, line); }
  LayoutUnit OffsetFromDecoratingBox() const;
  float ComputeThickness() const;
  float ComputeUnderlineThickness(
      const TextDecorationThickness& applied_decoration_thickness,
      const ComputedStyle* decorating_box_style) const;

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

  void UpdateForDecorationIndex();

  // The |ComputedStyle| of the target text/box to paint decorations for.
  const ComputedStyle& target_style_;
  // The |ComputedStyle| of the [decorating box]. Decorations are computed from
  // this style.
  // [decorating box]: https://drafts.csswg.org/css-text-decor-3/#decorating-box
  const ComputedStyle* decorating_box_style_ = nullptr;

  // Decorating box properties for the current |decoration_index_|.
  const NGInlinePaintContext* const inline_context_ = nullptr;
  const NGDecoratingBox* decorating_box_ = nullptr;
  const AppliedTextDecoration* applied_text_decoration_ = nullptr;
  const absl::optional<AppliedTextDecoration> selection_text_decoration_;
  const Font* font_ = nullptr;
  const SimpleFontData* font_data_ = nullptr;

  // These "overrides" fields force using the specified style or font instead
  // of the one from the decorating box. Note that using them means that the
  // [decorating box] is not supported.
  const Font* const font_override_ = nullptr;
  const ComputedStyle* const decorating_box_style_override_ = nullptr;
  const absl::optional<FontBaseline> baseline_type_override_;

  // Geometry of the target text/box.
  const PhysicalOffset local_origin_;
  const LayoutUnit width_;

  // Cached properties for the current |decoration_index_|.
  const float target_ascent_ = 0.f;
  float ascent_ = 0.f;
  float computed_font_size_ = 0.f;
  float resolved_thickness_ = 0.f;
  const float scaling_factor_;

  int decoration_index_ = 0;

  TextDecorationLine lines_ = TextDecorationLine::kNone;
  ResolvedUnderlinePosition original_underline_position_ =
      ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto;
  ResolvedUnderlinePosition flipped_underline_position_ =
      ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto;

  bool has_underline_ = false;
  bool has_overline_ = false;
  bool flip_underline_and_overline_ = false;
  bool use_decorating_box_ = false;
  const bool minimum_thickness_is_one_ = false;
  const bool antialias_ = false;

  struct LineData {
    TextDecorationLine line;
    float line_offset;
    float double_offset;
    int wavy_offset_factor;
    absl::optional<Path> stroke_path;
  };
  LineData line_data_;
  absl::optional<Color> highlight_override_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_INFO_H_
