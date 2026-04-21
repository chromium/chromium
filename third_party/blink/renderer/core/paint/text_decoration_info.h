// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_INFO_H_

#include <optional>

#include "base/types/strong_alias.h"
#include "cc/paint/paint_record.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/decoration_line_painter.h"
#include "third_party/blink/renderer/core/paint/line_relative_rect.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/graphics/styled_stroke_data.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class ComputedStyle;
class DecoratingBox;
class Font;
class InlinePaintContext;
class SimpleFontData;
class TextDecorationOffset;

enum class ResolvedUnderlinePosition {
  kNearAlphabeticBaselineAuto,
  kNearAlphabeticBaselineFromFont,
  kUnder,
  kOver
};

using IsSvgText = base::StrongAlias<class IsSvgTextTag, bool>;

// Holds the resolved metrics and styling for a single AppliedTextDecoration.
// This immutable structure decouples index-specific properties from the overall
// TextDecorationInfo context.
struct ResolvedDecoration {
  STACK_ALLOCATED();

 public:
  // ResolveDecorationAt() must fill `applied_text_decoration`, so it never be
  // nullptr.
  const AppliedTextDecoration* applied_text_decoration = nullptr;
  const SimpleFontData* font_data = nullptr;
  TextDecorationLine lines = TextDecorationLine::kNone;
  float ascent = 0.f;
  float computed_font_size = 0.f;
  float resolved_thickness = 0.f;
  float effective_zoom = 1.0f;
  // This field is available only if a decorating box is applied and `lines`
  // has underline.
  LayoutUnit offset_from_decorating_box;
  ResolvedUnderlinePosition underline_position =
      ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto;
  bool has_underline = false;
  bool has_overline = false;
  bool is_flipped_underline_and_overline = false;

  bool HasUnderline() const { return has_underline; }
  bool HasOverline() const { return has_overline; }
  bool HasLineThrough() const {
    return EnumHasFlags(lines, TextDecorationLine::kLineThrough);
  }
  bool HasSpellingError() const {
    return EnumHasFlags(lines, TextDecorationLine::kSpellingError);
  }
  bool HasGrammarError() const {
    return EnumHasFlags(lines, TextDecorationLine::kGrammarError);
  }
  bool HasSpellingOrGrammarError() const {
    return HasSpellingError() || HasGrammarError();
  }
};

// Container for computing and storing information for text decoration
// invalidation and painting. See also
// https://www.w3.org/TR/css-text-decor-3/#painting-order
class CORE_EXPORT TextDecorationInfo {
  STACK_ALLOCATED();

 public:
  TextDecorationInfo(LineRelativeOffset local_origin,
                     LayoutUnit width,
                     const ComputedStyle& target_style,
                     const InlinePaintContext* inline_context,
                     const TextDecorationLine selection_decoration_line,
                     const Color selection_decoration_color,
                     const AppliedTextDecoration* decoration_override = nullptr,
                     const Font* font_override = nullptr,
                     IsSvgText is_svg_text = IsSvgText(false),
                     float svg_resource_scaling_factor = 1.0f);

  wtf_size_t AppliedDecorationCount() const;
  const AppliedTextDecoration& AppliedDecoration(wtf_size_t) const;
  bool HasDecorationOverride() const { return !!decoration_override_; }

  // Returns whether any of the decoration indices in AppliedTextDecoration
  // have any of the given lines.
  bool HasAnyLine(TextDecorationLine lines) const {
    return EnumHasFlags(union_all_lines_, lines);
  }

  // Resolve the AppliedTextDecoration at the specified index.
  //
  // The index must be a valid index the AppliedTextDecorations contained within
  // the style passed at construction.
  const ResolvedDecoration ResolveDecorationAt(wtf_size_t decoration_index);

  // Creates a DecorationGeometry for one of the text decoration lines: over,
  // under, line-through, or spelling/grammar error. It's necessary to paint
  // or compute bounds for a line.
  DecorationGeometry ComputeLineData(const ResolvedDecoration& decoration,
                                     TextDecorationLine line,
                                     float line_offset) const;
  DecorationGeometry ComputeUnderlineLineData(
      const ResolvedDecoration& decoration,
      const TextDecorationOffset& decoration_offset) const;
  DecorationGeometry ComputeOverlineLineData(
      const ResolvedDecoration& decoration,
      const TextDecorationOffset& decoration_offset) const;
  DecorationGeometry ComputeLineThroughLineData(
      const ResolvedDecoration& decoration) const;
  DecorationGeometry ComputeSpellingOrGrammarErrorLineData(
      const ResolvedDecoration& decoration,
      const TextDecorationOffset&) const;

  const ComputedStyle& TargetStyle() const { return target_style_; }
  // Returns the scaling factor for the decoration.
  // It can be different from FragmentItem::SvgScalingFactor() if the
  // text works as a resource.
  float SvgResourceScalingFactor() const {
    return svg_resource_scaling_factor_;
  }
  float BaselineForInkSkip() const {
    return local_origin_.line_over.ToFloat() + target_ascent_;
  }

  Color LineColor(const ResolvedDecoration& decoration) const;

  // Overrides the line color with the given topmost active highlight ‘color’
  // (for originating decorations being painted in highlight overlays), or the
  // highlight ‘text-decoration-color’ resolved with the correct ‘currentColor’
  // (for decorations introduced by highlight pseudos).
  void SetHighlightOverrideColor(const std::optional<Color>&);

 private:
  LayoutUnit OffsetFromDecoratingBox(const DecoratingBox& decorating_box) const;
  float ComputeThickness(const ResolvedDecoration& decoration) const;

  const ResolvedDecoration UpdateForDecorationIndex();

  LayoutUnit Width() const { return width_; }

  // The |ComputedStyle| of the target text/box to paint decorations for.
  const ComputedStyle& target_style_;
  // The |ComputedStyle| of the [decorating box]. Decorations are computed from
  // this style.
  // [decorating box]: https://drafts.csswg.org/css-text-decor-3/#decorating-box
  const ComputedStyle* decorating_box_style_ = nullptr;

  const InlinePaintContext* const inline_context_ = nullptr;

  const TextDecorationLine selection_decoration_line_ =
      TextDecorationLine::kNone;
  const Color selection_decoration_color_;
  const Font* font_ = nullptr;

  // These "overrides" fields force using the specified style or font instead
  // of the one from the decorating box. Note that using them means that the
  // [decorating box] is not supported.
  const AppliedTextDecoration* const decoration_override_ = nullptr;
  const Font* const font_override_ = nullptr;

  // Geometry of the target text/box.
  const LineRelativeOffset local_origin_;
  const LayoutUnit width_;
  const float target_ascent_ = 0.f;
  const float svg_resource_scaling_factor_;

  wtf_size_t decoration_index_ = 0;

  // |union_all_lines_| represents the lines found in any |decoration_index_|.
  //
  // Ideally we would build a vector of the TextDecorationLine instances needing
  // ‘line-through’, but this is a rare case so better to avoid vector overhead.
  TextDecorationLine union_all_lines_ = TextDecorationLine::kNone;

  ResolvedUnderlinePosition original_underline_position_ =
      ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto;

  bool flip_underline_and_overline_ = false;
  bool use_decorating_box_ = false;
  const bool is_svg_text_ = false;
  bool antialias_ = false;

  std::optional<Color> highlight_override_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_INFO_H_
