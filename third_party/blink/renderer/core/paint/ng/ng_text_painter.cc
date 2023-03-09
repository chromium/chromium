// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"

#include "base/auto_reset.h"
#include "base/types/optional_util.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/ng_text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

class SelectionStyleScope {
  STACK_ALLOCATED();

 public:
  SelectionStyleScope(const LayoutObject&,
                      const ComputedStyle& style,
                      const ComputedStyle& selection_style);
  SelectionStyleScope(const SelectionStyleScope&) = delete;
  SelectionStyleScope& operator=(const SelectionStyleScope) = delete;
  ~SelectionStyleScope();

 private:
  const LayoutObject& layout_object_;
  const ComputedStyle& selection_style_;
  const bool styles_are_equal_;
};

SelectionStyleScope::SelectionStyleScope(const LayoutObject& layout_object,
                                         const ComputedStyle& style,
                                         const ComputedStyle& selection_style)
    : layout_object_(layout_object),
      selection_style_(selection_style),
      styles_are_equal_(style == selection_style) {
  if (styles_are_equal_)
    return;
  DCHECK(!layout_object.IsSVGInlineText());
  SVGResources::UpdatePaints(layout_object_, nullptr, selection_style_);
}

SelectionStyleScope::~SelectionStyleScope() {
  if (styles_are_equal_)
    return;
  SVGResources::ClearPaints(layout_object_, &selection_style_);
}

enum class SvgPaintMode { kText, kTextDecoration };

const ComputedStyle& GetSvgStyleToPaint(
    const NGTextPainter::SvgTextPaintState& state,
    SvgPaintMode svg_paint_mode,
    absl::optional<SelectionStyleScope>& selection_style_scope,
    bool& out_has_fill,
    bool& out_has_visible_stroke) {
  // https://svgwg.org/svg2-draft/text.html#TextDecorationProperties
  // The fill and stroke of the text decoration are given by the fill and stroke
  // of the text at the point where the text decoration is declared.
  const LayoutObject& layout_parent = svg_paint_mode == SvgPaintMode::kText
                                          ? *state.InlineText().Parent()
                                          : state.TextDecorationObject();

  const ComputedStyle* style_to_paint = layout_parent.Style();
  out_has_fill = style_to_paint->HasFill();
  out_has_visible_stroke = style_to_paint->HasVisibleStroke();
  if (state.IsPaintingSelection()) {
    if (const ComputedStyle* pseudo_selection_style =
            layout_parent.GetSelectionStyle()) {
      style_to_paint = pseudo_selection_style;
      if (!out_has_fill)
        out_has_fill = style_to_paint->HasFill();
      if (!out_has_visible_stroke)
        out_has_visible_stroke = style_to_paint->HasVisibleStroke();
    }

    selection_style_scope.emplace(layout_parent, *layout_parent.Style(),
                                  *style_to_paint);
  }

  if (state.IsRenderingClipPathAsMaskImage()) {
    out_has_fill = true;
    out_has_visible_stroke = false;
  }

  return *style_to_paint;
}

bool SetupPaintForSvgText(const NGTextPainter::SvgTextPaintState& state,
                          const GraphicsContext& context,
                          const ComputedStyle& style,
                          SvgPaintMode svg_paint_mode,
                          LayoutSVGResourceMode resource_mode,
                          cc::PaintFlags& flags) {
  const LayoutObject& layout_parent = svg_paint_mode == SvgPaintMode::kText
                                          ? *state.InlineText().Parent()
                                          : state.TextDecorationObject();
  if (!SVGObjectPainter(layout_parent)
           .PreparePaint(state.IsRenderingClipPathAsMaskImage(), style,
                         resource_mode, flags, state.GetShaderTransform())) {
    return false;
  }

  flags.setAntiAlias(true);

  if (style.TextShadow() &&
      // Text shadows are disabled for clip-paths, because they are not
      // geometry.
      !state.IsRenderingClipPathAsMaskImage() &&
      // Text shadows are disabled when printing. http://crbug.com/258321
      !layout_parent.GetDocument().Printing()) {
    flags.setLooper(TextPainterBase::CreateDrawLooper(
        style.TextShadow(), DrawLooperBuilder::kShadowRespectsAlpha,
        style.VisitedDependentColor(GetCSSPropertyColor()),
        style.UsedColorScheme()));
  }

  if (resource_mode == kApplyToStrokeMode) {
    // The stroke geometry needs be generated based on the scaled font.
    float stroke_scale_factor = 1;
    if (style.VectorEffect() != EVectorEffect::kNonScalingStroke) {
      switch (svg_paint_mode) {
        case SvgPaintMode::kText: {
          stroke_scale_factor = state.InlineText().ScalingFactor();
          break;
        }
        case SvgPaintMode::kTextDecoration: {
          Font scaled_font;
          LayoutSVGInlineText::ComputeNewScaledFontForStyle(
              layout_parent, stroke_scale_factor, scaled_font);
          DCHECK(stroke_scale_factor);
          break;
        }
      }
    }

    StrokeData stroke_data;
    SVGLayoutSupport::ApplyStrokeStyleToStrokeData(
        stroke_data, style, layout_parent, stroke_scale_factor);
    if (stroke_scale_factor != 1)
      stroke_data.SetThickness(stroke_data.Thickness() * stroke_scale_factor);
    stroke_data.SetupPaint(&flags);
  }

  return true;
}

}  // namespace

void NGTextPainter::Paint(const NGTextFragmentPaintInfo& fragment_paint_info,
                          unsigned length,
                          const TextPaintStyle& text_style,
                          DOMNodeId node_id,
                          const AutoDarkMode& auto_dark_mode,
                          ShadowMode shadow_mode) {
  GraphicsContextStateSaver state_saver(graphics_context_, false);
  UpdateGraphicsContext(graphics_context_, text_style, state_saver,
                        shadow_mode);
  // TODO(layout-dev): Handle combine text here or elsewhere.
  PaintInternal<kPaintText>(fragment_paint_info, length, node_id,
                            auto_dark_mode);

  if (!emphasis_mark_.empty()) {
    if (text_style.emphasis_mark_color != text_style.fill_color)
      graphics_context_.SetFillColor(text_style.emphasis_mark_color);
    PaintInternal<kPaintEmphasisMark>(fragment_paint_info, length, node_id,
                                      auto_dark_mode);
  }
}

// This function paints text twice with different styles in order to:
// 1. Paint glyphs inside of |selection_rect| using |selection_style|, and
//    outside using |text_style|.
// 2. Paint parts of a ligature glyph.
void NGTextPainter::PaintSelectedText(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    unsigned selection_start,
    unsigned selection_end,
    unsigned length,
    const TextPaintStyle& text_style,
    const TextPaintStyle& selection_style,
    const PhysicalRect& selection_rect,
    DOMNodeId node_id,
    const AutoDarkMode& auto_dark_mode) {
  if (!fragment_paint_info.shape_result)
    return;

  // Use fast path if all glyphs fit in |selection_rect|. |visual_rect_| is the
  // ink bounds of all glyphs of this text fragment, including characters before
  // |start_offset| or after |end_offset|. Computing exact bounds is expensive
  // that this code only checks bounds of all glyphs.
  gfx::Rect snapped_selection_rect(ToPixelSnappedRect(selection_rect));
  // Allowing 1px overflow is almost unnoticeable, while it can avoid two-pass
  // painting in most small text.
  snapped_selection_rect.Outset(1);
  // For SVG text, comparing with visual_rect_ does not work well because
  // selection_rect is in the scaled coordinate system and visual_rect_ is
  // in the unscaled coordinate system. Checks text offsets too.
  if (snapped_selection_rect.Contains(visual_rect_) ||
      (selection_start == fragment_paint_info.from &&
       selection_end == fragment_paint_info.to)) {
    absl::optional<base::AutoReset<bool>> is_painting_selection_reset;
    if (NGTextPainter::SvgTextPaintState* state = GetSvgState())
      is_painting_selection_reset.emplace(&state->is_painting_selection_, true);
    Paint(fragment_paint_info.Slice(selection_start, selection_end), length,
          selection_style, node_id, auto_dark_mode);
    return;
  }

  // Adjust start/end offset when they are in the middle of a ligature. e.g.,
  // when |start_offset| is between a ligature of "fi", it needs to be adjusted
  // to before "f".
  fragment_paint_info.shape_result->ExpandRangeToIncludePartialGlyphs(
      &selection_start, &selection_end);

  // Because only a part of the text glyph can be selected, we need to draw
  // the selection twice. First, draw the glyphs outside the selection area,
  // with the original style.
  gfx::RectF float_selection_rect(selection_rect);
  {
    GraphicsContextStateSaver state_saver(graphics_context_);
    graphics_context_.ClipOut(float_selection_rect);
    Paint(fragment_paint_info.Slice(selection_start, selection_end), length,
          text_style, node_id, auto_dark_mode, kTextProperOnly);
  }
  // Then draw the glyphs inside the selection area, with the selection style.
  {
    absl::optional<base::AutoReset<bool>> is_painting_selection_reset;
    if (NGTextPainter::SvgTextPaintState* state = GetSvgState())
      is_painting_selection_reset.emplace(&state->is_painting_selection_, true);
    GraphicsContextStateSaver state_saver(graphics_context_);
    graphics_context_.Clip(float_selection_rect);
    Paint(fragment_paint_info.Slice(selection_start, selection_end), length,
          selection_style, node_id, auto_dark_mode);
  }
}

void NGTextPainter::PaintDecorationsExceptLineThrough(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    const NGFragmentItem& text_item,
    const PaintInfo& paint_info,
    const ComputedStyle& style,
    const TextPaintStyle& text_style,
    TextDecorationInfo& decoration_info,
    TextDecorationLine lines_to_paint) {
  if (!decoration_info.HasAnyLine(lines_to_paint &
                                  ~TextDecorationLine::kLineThrough))
    return;

  const NGTextDecorationOffset decoration_offset(decoration_info.TargetStyle(),
                                                 text_item.Style());

  if (svg_text_paint_state_.has_value() &&
      !decoration_info.HasDecorationOverride()) {
    GraphicsContextStateSaver state_saver(paint_info.context, false);
    if (paint_info.IsRenderingResourceSubtree()) {
      state_saver.SaveIfNeeded();
      paint_info.context.Scale(
          1, text_item.SvgScalingFactor() / decoration_info.ScalingFactor());
    }
    PaintSvgDecorationsExceptLineThrough(fragment_paint_info, decoration_offset,
                                         decoration_info, lines_to_paint,
                                         paint_info, text_style);
  } else {
    NGTextPainterBase::PaintUnderOrOverLineDecorations(
        fragment_paint_info, decoration_offset, decoration_info, lines_to_paint,
        paint_info, text_style, nullptr);
  }
}

void NGTextPainter::PaintDecorationsOnlyLineThrough(
    const NGFragmentItem& text_item,
    const PaintInfo& paint_info,
    const ComputedStyle& style,
    const TextPaintStyle& text_style,
    TextDecorationInfo& decoration_info) {
  if (!decoration_info.HasAnyLine(TextDecorationLine::kLineThrough))
    return;

  if (svg_text_paint_state_.has_value() &&
      !decoration_info.HasDecorationOverride()) {
    GraphicsContextStateSaver state_saver(paint_info.context, false);
    if (paint_info.IsRenderingResourceSubtree()) {
      state_saver.SaveIfNeeded();
      paint_info.context.Scale(
          1, text_item.SvgScalingFactor() / decoration_info.ScalingFactor());
    }
    PaintSvgDecorationsOnlyLineThrough(decoration_info, paint_info, text_style);
  } else {
    TextPainterBase::PaintDecorationsOnlyLineThrough(decoration_info,
                                                     paint_info, text_style);
  }
}

template <NGTextPainter::PaintInternalStep step>
void NGTextPainter::PaintInternalFragment(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    DOMNodeId node_id,
    const AutoDarkMode& auto_dark_mode) {
  DCHECK(fragment_paint_info.from <= fragment_paint_info.text.length());
  DCHECK(fragment_paint_info.to <= fragment_paint_info.text.length());

  if (step == kPaintEmphasisMark) {
    graphics_context_.DrawEmphasisMarks(
        font_, fragment_paint_info, emphasis_mark_,
        gfx::PointF(text_origin_) + gfx::Vector2dF(0, emphasis_mark_offset_),
        auto_dark_mode);
  } else {
    DCHECK(step == kPaintText);
    if (svg_text_paint_state_.has_value()) {
      AutoDarkMode svg_text_auto_dark_mode(DarkModeFilter::ElementRole::kSVG,
                                           auto_dark_mode.enabled);
      PaintSvgTextFragment(fragment_paint_info, node_id,
                           svg_text_auto_dark_mode);
    } else {
      graphics_context_.DrawText(font_, fragment_paint_info,
                                 gfx::PointF(text_origin_), node_id,
                                 auto_dark_mode);
    }

    // TODO(sohom): SubstringContainsOnlyWhitespaceOrEmpty() does not check
    // for all whitespace characters as defined in the spec definition of
    // whitespace. See https://w3c.github.io/paint-timing/#non-empty
    // In particular 0xb and 0xc are not checked.
    if (!fragment_paint_info.text.SubstringContainsOnlyWhitespaceOrEmpty(
            fragment_paint_info.from, fragment_paint_info.to))
      graphics_context_.GetPaintController().SetTextPainted();

    if (!font_.ShouldSkipDrawing())
      PaintTimingDetector::NotifyTextPaint(visual_rect_);
  }
}

template <NGTextPainter::PaintInternalStep Step>
void NGTextPainter::PaintInternal(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    unsigned truncation_point,
    DOMNodeId node_id,
    const AutoDarkMode& auto_dark_mode) {
  // TODO(layout-dev): We shouldn't be creating text fragments without text.
  if (!fragment_paint_info.shape_result)
    return;

  if (fragment_paint_info.from <= fragment_paint_info.to) {
    PaintInternalFragment<Step>(fragment_paint_info, node_id, auto_dark_mode);
  } else {
    if (fragment_paint_info.to > 0) {
      PaintInternalFragment<Step>(
          fragment_paint_info.WithStartOffset(ellipsis_offset_), node_id,
          auto_dark_mode);
    }
    if (fragment_paint_info.from < truncation_point) {
      PaintInternalFragment<Step>(
          fragment_paint_info.WithEndOffset(truncation_point), node_id,
          auto_dark_mode);
    }
  }
}

void NGTextPainter::ClipDecorationsStripe(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    float upper,
    float stripe_width,
    float dilation) {
  if (fragment_paint_info.from >= fragment_paint_info.to ||
      !fragment_paint_info.shape_result)
    return;

  Vector<Font::TextIntercept> text_intercepts;
  font_.GetTextIntercepts(fragment_paint_info, graphics_context_.FillFlags(),
                          std::make_tuple(upper, upper + stripe_width),
                          text_intercepts);

  DecorationsStripeIntercepts(upper, stripe_width, dilation, text_intercepts);
}

void NGTextPainter::PaintSvgTextFragment(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    DOMNodeId node_id,
    const AutoDarkMode& auto_dark_mode) {
  const NGTextPainter::SvgTextPaintState& state = svg_text_paint_state_.value();
  if (state.IsPaintingTextMatch()) {
    cc::PaintFlags fill_flags;
    fill_flags.setColor(state.TextMatchColor().Rgb());
    fill_flags.setAntiAlias(true);

    cc::PaintFlags stroke_flags;
    bool should_paint_stroke = false;
    if (SetupPaintForSvgText(state, graphics_context_, state.Style(),
                             SvgPaintMode::kText, kApplyToStrokeMode,
                             stroke_flags)) {
      should_paint_stroke = true;
      stroke_flags.setLooper(nullptr);
      stroke_flags.setColor(state.TextMatchColor().Rgb());
    }
    graphics_context_.DrawText(font_, fragment_paint_info,
                               gfx::PointF(text_origin_), fill_flags, node_id,
                               auto_dark_mode);
    if (should_paint_stroke) {
      graphics_context_.DrawText(font_, fragment_paint_info,
                                 gfx::PointF(text_origin_), stroke_flags,
                                 node_id, auto_dark_mode);
    }
    return;
  }

  absl::optional<SelectionStyleScope> selection_style_scope;
  bool has_fill = false;
  bool has_visible_stroke = false;
  const ComputedStyle& style_to_paint =
      GetSvgStyleToPaint(state, SvgPaintMode::kText, selection_style_scope,
                         has_fill, has_visible_stroke);

  for (int i = 0; i < 3; i++) {
    absl::optional<LayoutSVGResourceMode> resource_mode;

    switch (state.Style().PaintOrderType(i)) {
      case PT_FILL:
        if (has_fill)
          resource_mode = kApplyToFillMode;
        break;
      case PT_STROKE:
        if (has_visible_stroke)
          resource_mode = kApplyToStrokeMode;
        break;
      case PT_MARKERS:
        // Markers don't apply to text
        break;
      default:
        NOTREACHED();
        break;
    }

    if (resource_mode) {
      cc::PaintFlags flags;
      if (SetupPaintForSvgText(state, graphics_context_, style_to_paint,
                               SvgPaintMode::kText, *resource_mode, flags)) {
        graphics_context_.DrawText(font_, fragment_paint_info,
                                   gfx::PointF(text_origin_), flags, node_id,
                                   auto_dark_mode);
      }
    }
  }
}

void NGTextPainter::PaintSvgDecorationsExceptLineThrough(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    const TextDecorationOffsetBase& decoration_offset,
    TextDecorationInfo& decoration_info,
    TextDecorationLine lines_to_paint,
    const PaintInfo& paint_info,
    const TextPaintStyle& text_style) {
  const NGTextPainter::SvgTextPaintState& state = svg_text_paint_state_.value();
  absl::optional<SelectionStyleScope> selection_style_scope;
  bool has_fill = false;
  bool has_visible_stroke = false;
  const ComputedStyle& style_to_paint =
      GetSvgStyleToPaint(state, SvgPaintMode::kTextDecoration,
                         selection_style_scope, has_fill, has_visible_stroke);

  for (int i = 0; i < 3; i++) {
    absl::optional<LayoutSVGResourceMode> resource_mode;

    switch (state.Style().PaintOrderType(i)) {
      case PT_FILL:
        if (has_fill)
          resource_mode = kApplyToFillMode;
        break;
      case PT_STROKE:
        if (has_visible_stroke)
          resource_mode = kApplyToStrokeMode;
        break;
      case PT_MARKERS:
        // Markers don't apply to text-decorations
        break;
      default:
        NOTREACHED();
        break;
    }

    if (resource_mode) {
      cc::PaintFlags flags;
      if (SetupPaintForSvgText(state, graphics_context_, style_to_paint,
                               SvgPaintMode::kTextDecoration, *resource_mode,
                               flags)) {
        NGTextPainterBase::PaintUnderOrOverLineDecorations(
            fragment_paint_info, decoration_offset, decoration_info,
            lines_to_paint, paint_info, text_style, &flags);
      }
    }
  }
}

void NGTextPainter::PaintSvgDecorationsOnlyLineThrough(
    TextDecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const TextPaintStyle& text_style) {
  const NGTextPainter::SvgTextPaintState& state = svg_text_paint_state_.value();
  absl::optional<SelectionStyleScope> selection_style_scope;
  bool has_fill = false;
  bool has_visible_stroke = false;
  const ComputedStyle& style_to_paint =
      GetSvgStyleToPaint(state, SvgPaintMode::kTextDecoration,
                         selection_style_scope, has_fill, has_visible_stroke);

  for (int i = 0; i < 3; i++) {
    absl::optional<LayoutSVGResourceMode> resource_mode;

    switch (state.Style().PaintOrderType(i)) {
      case PT_FILL:
        if (has_fill)
          resource_mode = kApplyToFillMode;
        break;
      case PT_STROKE:
        if (has_visible_stroke)
          resource_mode = kApplyToStrokeMode;
        break;
      case PT_MARKERS:
        // Markers don't apply to text-decorations
        break;
      default:
        NOTREACHED();
        break;
    }

    if (resource_mode) {
      cc::PaintFlags flags;
      if (SetupPaintForSvgText(state, graphics_context_, style_to_paint,
                               SvgPaintMode::kTextDecoration, *resource_mode,
                               flags)) {
        TextPainterBase::PaintDecorationsOnlyLineThrough(
            decoration_info, paint_info, text_style, &flags);
      }
    }
  }
}

NGTextPainter::SvgTextPaintState& NGTextPainter::SetSvgState(
    const LayoutSVGInlineText& svg_inline_text,
    const ComputedStyle& style,
    NGStyleVariant style_variant,
    bool is_rendering_clip_path_as_mask_image) {
  return svg_text_paint_state_.emplace(svg_inline_text, style, style_variant,
                                       is_rendering_clip_path_as_mask_image);
}

NGTextPainter::SvgTextPaintState& NGTextPainter::SetSvgState(
    const LayoutSVGInlineText& svg_inline_text,
    const ComputedStyle& style,
    Color text_match_color) {
  return svg_text_paint_state_.emplace(svg_inline_text, style,
                                       text_match_color);
}

NGTextPainter::SvgTextPaintState* NGTextPainter::GetSvgState() {
  return base::OptionalToPtr(svg_text_paint_state_);
}

NGTextPainter::SvgTextPaintState::SvgTextPaintState(
    const LayoutSVGInlineText& layout_svg_inline_text,
    const ComputedStyle& style,
    NGStyleVariant style_variant,
    bool is_rendering_clip_path_as_mask_image)
    : layout_svg_inline_text_(layout_svg_inline_text),
      style_(style),
      style_variant_(style_variant),
      is_rendering_clip_path_as_mask_image_(
          is_rendering_clip_path_as_mask_image) {}

NGTextPainter::SvgTextPaintState::SvgTextPaintState(
    const LayoutSVGInlineText& layout_svg_inline_text,
    const ComputedStyle& style,
    Color text_match_color)
    : layout_svg_inline_text_(layout_svg_inline_text),
      style_(style),
      text_match_color_(text_match_color) {}

const LayoutSVGInlineText& NGTextPainter::SvgTextPaintState::InlineText()
    const {
  return layout_svg_inline_text_;
}

const LayoutObject& NGTextPainter::SvgTextPaintState::TextDecorationObject()
    const {
  // Lookup the first LayoutObject in parent hierarchy which has text-decoration
  // set.
  const LayoutObject* result = InlineText().Parent();
  while (result) {
    if (style_variant_ == NGStyleVariant::kFirstLine) {
      if (const ComputedStyle* style = result->FirstLineStyle()) {
        if (style->GetTextDecorationLine() != TextDecorationLine::kNone)
          break;
      }
    }
    if (const ComputedStyle* style = result->Style()) {
      if (style->GetTextDecorationLine() != TextDecorationLine::kNone)
        break;
    }

    result = result->Parent();
  }

  DCHECK(result);
  return *result;
}

const ComputedStyle& NGTextPainter::SvgTextPaintState::Style() const {
  return style_;
}

bool NGTextPainter::SvgTextPaintState::IsPaintingSelection() const {
  return is_painting_selection_;
}

bool NGTextPainter::SvgTextPaintState::IsRenderingClipPathAsMaskImage() const {
  return is_rendering_clip_path_as_mask_image_;
}

bool NGTextPainter::SvgTextPaintState::IsPaintingTextMatch() const {
  return text_match_color_.has_value();
}

Color NGTextPainter::SvgTextPaintState::TextMatchColor() const {
  return *text_match_color_;
}

AffineTransform& NGTextPainter::SvgTextPaintState::EnsureShaderTransform() {
  return shader_transform_ ? shader_transform_.value()
                           : shader_transform_.emplace();
}

const AffineTransform* NGTextPainter::SvgTextPaintState::GetShaderTransform()
    const {
  return base::OptionalToPtr(shader_transform_);
}

}  // namespace blink
