// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_painter.h"

#include "base/auto_reset.h"
#include "base/types/optional_util.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/decoration_line_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/paint_order_array.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
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

void PrepareStrokeGeometry(const TextPainter::SvgTextPaintState& state,
                           const ComputedStyle& style,
                           const LayoutObject& layout_parent,
                           SvgPaintMode svg_paint_mode,
                           cc::PaintFlags& flags) {
  float stroke_scale_factor = 1;
  // The stroke geometry needs be generated based on the scaled font.
  if (style.VectorEffect() != EVectorEffect::kNonScalingStroke) {
    switch (svg_paint_mode) {
      case SvgPaintMode::kText:
        stroke_scale_factor = state.InlineText().ScalingFactor();
        break;
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
  if (stroke_scale_factor != 1) {
    stroke_data.SetThickness(stroke_data.Thickness() * stroke_scale_factor);
  }
  stroke_data.SetupPaint(&flags);
}

const ShadowList* GetTextShadows(const ComputedStyle& style,
                                 const LayoutObject& layout_parent) {
  // Text shadows are disabled when printing. http://crbug.com/258321
  if (layout_parent.GetDocument().Printing()) {
    return nullptr;
  }
  return style.TextShadow();
}

void PrepareTextShadow(const ShadowList* text_shadows,
                       const ComputedStyle& style,
                       cc::PaintFlags& flags) {
  if (!text_shadows) {
    return;
  }
  flags.setLooper(TextPainterBase::CreateDrawLooper(
      text_shadows, DrawLooperBuilder::kShadowRespectsAlpha,
      style.VisitedDependentColor(GetCSSPropertyColor()),
      style.UsedColorScheme(), TextPainterBase::kBothShadowsAndTextProper));
}

struct SvgPaints {
  std::optional<cc::PaintFlags> fill;
  std::optional<cc::PaintFlags> stroke;
};

void PrepareSvgPaints(const TextPainter::SvgTextPaintState& state,
                      const SvgContextPaints* context_paints,
                      SvgPaintMode paint_mode,
                      SvgPaints& paints) {
  if (UNLIKELY(state.IsRenderingClipPathAsMaskImage())) {
    cc::PaintFlags& flags = paints.fill.emplace();
    flags.setColor(SK_ColorBLACK);
    flags.setAntiAlias(true);
    return;
  }

  // https://svgwg.org/svg2-draft/text.html#TextDecorationProperties
  // The fill and stroke of the text decoration are given by the fill and stroke
  // of the text at the point where the text decoration is declared.
  const LayoutObject& layout_parent = paint_mode == SvgPaintMode::kText
                                          ? *state.InlineText().Parent()
                                          : state.TextDecorationObject();
  SVGObjectPainter object_painter(layout_parent, context_paints);
  if (UNLIKELY(state.IsPaintingTextMatch())) {
    const ComputedStyle& style = state.Style();

    cc::PaintFlags& fill_flags = paints.fill.emplace();
    fill_flags.setColor(state.TextMatchColor().Rgb());
    fill_flags.setAntiAlias(true);

    cc::PaintFlags unused_flags;
    if (SVGObjectPainter::HasVisibleStroke(style, context_paints)) {
      if (!object_painter.PreparePaint(state.GetPaintFlags(), style,
                                       kApplyToStrokeMode, unused_flags)) {
        return;
      }
      cc::PaintFlags& stroke_flags = paints.stroke.emplace(fill_flags);
      PrepareStrokeGeometry(state, style, layout_parent, paint_mode,
                            stroke_flags);
    }
    return;
  }

  const ComputedStyle& style = [&layout_parent,
                                &state]() -> const ComputedStyle& {
    if (state.IsPaintingSelection()) {
      if (const ComputedStyle* pseudo_selection_style =
              layout_parent.GetSelectionStyle()) {
        return *pseudo_selection_style;
      }
    }
    return layout_parent.StyleRef();
  }();

  std::optional<SelectionStyleScope> paint_resource_scope;
  if (&style != layout_parent.Style()) {
    paint_resource_scope.emplace(layout_parent, *layout_parent.Style(), style);
  }

  const ShadowList* text_shadows = GetTextShadows(style, layout_parent);
  const AffineTransform* shader_transform = state.GetShaderTransform();
  if (SVGObjectPainter::HasFill(style, context_paints)) {
    if (object_painter.PreparePaint(state.GetPaintFlags(), style,
                                    kApplyToFillMode, paints.fill.emplace(),
                                    shader_transform)) {
      PrepareTextShadow(text_shadows, style, *paints.fill);
      paints.fill->setAntiAlias(true);
    } else {
      paints.fill.reset();
    }
  }
  if (SVGObjectPainter::HasVisibleStroke(style, context_paints)) {
    if (object_painter.PreparePaint(state.GetPaintFlags(), style,
                                    kApplyToStrokeMode, paints.stroke.emplace(),
                                    shader_transform)) {
      PrepareTextShadow(text_shadows, style, *paints.stroke);
      paints.stroke->setAntiAlias(true);

      PrepareStrokeGeometry(state, style, layout_parent, paint_mode,
                            *paints.stroke);
    } else {
      paints.stroke.reset();
    }
  }
}

using OrderedPaints = std::array<const cc::PaintFlags*, 2>;

OrderedPaints OrderPaints(const SvgPaints& paints, EPaintOrder paint_order) {
  OrderedPaints ordered_paints = {
      base::OptionalToPtr(paints.fill),
      base::OptionalToPtr(paints.stroke),
  };
  const PaintOrderArray paint_order_array(paint_order,
                                          PaintOrderArray::Type::kNoMarkers);
  if (paint_order_array[0] == PT_STROKE) {
    std::swap(ordered_paints[0], ordered_paints[1]);
  }
  return ordered_paints;
}

template <typename PassFunction>
void DrawPaintOrderPasses(const OrderedPaints& ordered_paints,
                          PassFunction pass) {
  for (const auto* paint : ordered_paints) {
    if (!paint) {
      continue;
    }
    pass(*paint);
  }
}

}  // namespace

void TextPainter::Paint(const TextFragmentPaintInfo& fragment_paint_info,
                        const TextPaintStyle& text_style,
                        DOMNodeId node_id,
                        const AutoDarkMode& auto_dark_mode,
                        ShadowMode shadow_mode) {
  // TODO(layout-dev): We shouldn't be creating text fragments without text.
  if (!fragment_paint_info.shape_result) {
    return;
  }
  DCHECK_LE(fragment_paint_info.from, fragment_paint_info.text.length());
  DCHECK_LE(fragment_paint_info.to, fragment_paint_info.text.length());

  GraphicsContextStateSaver state_saver(graphics_context_, false);
  UpdateGraphicsContext(graphics_context_, text_style, state_saver,
                        shadow_mode);
  // TODO(layout-dev): Handle combine text here or elsewhere.
  if (svg_text_paint_state_.has_value()) {
    const AutoDarkMode svg_text_auto_dark_mode(
        DarkModeFilter::ElementRole::kSVG,
        auto_dark_mode.enabled &&
            !svg_text_paint_state_->IsRenderingClipPathAsMaskImage());
    PaintSvgTextFragment(fragment_paint_info, node_id, svg_text_auto_dark_mode);
  } else {
    graphics_context_.DrawText(font_, fragment_paint_info,
                               gfx::PointF(text_origin_), node_id,
                               auto_dark_mode);
  }

  if (!emphasis_mark_.empty()) {
    if (text_style.emphasis_mark_color != text_style.fill_color)
      graphics_context_.SetFillColor(text_style.emphasis_mark_color);
    graphics_context_.DrawEmphasisMarks(
        font_, fragment_paint_info, emphasis_mark_,
        gfx::PointF(text_origin_) + gfx::Vector2dF(0, emphasis_mark_offset_),
        auto_dark_mode);
  }

  // TODO(sohom): SubstringContainsOnlyWhitespaceOrEmpty() does not check
  // for all whitespace characters as defined in the spec definition of
  // whitespace. See https://w3c.github.io/paint-timing/#non-empty
  // In particular 0xb and 0xc are not checked.
  if (!fragment_paint_info.text.SubstringContainsOnlyWhitespaceOrEmpty(
          fragment_paint_info.from, fragment_paint_info.to)) {
    graphics_context_.GetPaintController().SetTextPainted();
  }

  if (!font_.ShouldSkipDrawing()) {
    PaintTimingDetector::NotifyTextPaint(visual_rect_);
  }
}

// This function paints text twice with different styles in order to:
// 1. Paint glyphs inside of |selection_rect| using |selection_style|, and
//    outside using |text_style|.
// 2. Paint parts of a ligature glyph.
void TextPainter::PaintSelectedText(
    const TextFragmentPaintInfo& fragment_paint_info,
    unsigned selection_start,
    unsigned selection_end,
    const TextPaintStyle& text_style,
    const TextPaintStyle& selection_style,
    const LineRelativeRect& selection_rect,
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
    std::optional<base::AutoReset<bool>> is_painting_selection_reset;
    if (TextPainter::SvgTextPaintState* state = GetSvgState()) {
      is_painting_selection_reset.emplace(&state->is_painting_selection_, true);
    }
    Paint(fragment_paint_info.Slice(selection_start, selection_end),
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
    Paint(fragment_paint_info.Slice(selection_start, selection_end), text_style,
          node_id, auto_dark_mode, kTextProperOnly);
  }
  // Then draw the glyphs inside the selection area, with the selection style.
  {
    std::optional<base::AutoReset<bool>> is_painting_selection_reset;
    if (TextPainter::SvgTextPaintState* state = GetSvgState()) {
      is_painting_selection_reset.emplace(&state->is_painting_selection_, true);
    }
    GraphicsContextStateSaver state_saver(graphics_context_);
    graphics_context_.Clip(float_selection_rect);
    Paint(fragment_paint_info.Slice(selection_start, selection_end),
          selection_style, node_id, auto_dark_mode);
  }
}

void TextPainter::PaintDecorationLine(
    const TextDecorationInfo& decoration_info,
    const Color& line_color,
    const TextFragmentPaintInfo* fragment_paint_info) {
  DecorationLinePainter decoration_painter(graphics_context_, decoration_info);
  if (fragment_paint_info &&
      decoration_info.TargetStyle().TextDecorationSkipInk() ==
          ETextDecorationSkipInk::kAuto) {
    // In order to ignore intersects less than 0.5px, inflate by -0.5.
    gfx::RectF decoration_bounds = decoration_info.Bounds();
    decoration_bounds.Inset(gfx::InsetsF::VH(0.5, 0));
    ClipDecorationsStripe(
        *fragment_paint_info,
        decoration_info.InkSkipClipUpper(decoration_bounds.y()),
        decoration_bounds.height(),
        std::min(decoration_info.ResolvedThickness(),
                 kDecorationClipMaxDilation));
  }

  if (svg_text_paint_state_.has_value() &&
      !decoration_info.HasDecorationOverride()) {
    SvgPaints paints;
    const SvgTextPaintState& state = svg_text_paint_state_.value();
    PrepareSvgPaints(state, svg_context_paints_, SvgPaintMode::kTextDecoration,
                     paints);

    const OrderedPaints ordered_paints =
        OrderPaints(paints, state.Style().PaintOrder());
    DrawPaintOrderPasses(ordered_paints, [&](const cc::PaintFlags& flags) {
      decoration_painter.Paint(line_color, &flags);
    });
  } else {
    decoration_painter.Paint(line_color, nullptr);
  }
}

void TextPainter::ClipDecorationsStripe(
    const TextFragmentPaintInfo& fragment_paint_info,
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

void TextPainter::PaintSvgTextFragment(
    const TextFragmentPaintInfo& fragment_paint_info,
    DOMNodeId node_id,
    const AutoDarkMode& auto_dark_mode) {
  SvgPaints paints;
  const SvgTextPaintState& state = svg_text_paint_state_.value();
  PrepareSvgPaints(state, svg_context_paints_, SvgPaintMode::kText, paints);

  const OrderedPaints ordered_paints =
      OrderPaints(paints, state.Style().PaintOrder());
  DrawPaintOrderPasses(ordered_paints, [&](const cc::PaintFlags& flags) {
    graphics_context_.DrawText(font_, fragment_paint_info,
                               gfx::PointF(text_origin_), flags, node_id,
                               auto_dark_mode);
  });
}

TextPainter::SvgTextPaintState& TextPainter::SetSvgState(
    const LayoutSVGInlineText& svg_inline_text,
    const ComputedStyle& style,
    StyleVariant style_variant,
    PaintFlags paint_flags) {
  return svg_text_paint_state_.emplace(svg_inline_text, style, style_variant,
                                       paint_flags);
}

TextPainter::SvgTextPaintState& TextPainter::SetSvgState(
    const LayoutSVGInlineText& svg_inline_text,
    const ComputedStyle& style,
    Color text_match_color) {
  return svg_text_paint_state_.emplace(svg_inline_text, style,
                                       text_match_color);
}

TextPainter::SvgTextPaintState* TextPainter::GetSvgState() {
  return base::OptionalToPtr(svg_text_paint_state_);
}

TextPainter::SvgTextPaintState::SvgTextPaintState(
    const LayoutSVGInlineText& layout_svg_inline_text,
    const ComputedStyle& style,
    StyleVariant style_variant,
    PaintFlags paint_flags)
    : layout_svg_inline_text_(layout_svg_inline_text),
      style_(style),
      style_variant_(style_variant),
      paint_flags_(paint_flags) {}

TextPainter::SvgTextPaintState::SvgTextPaintState(
    const LayoutSVGInlineText& layout_svg_inline_text,
    const ComputedStyle& style,
    Color text_match_color)
    : layout_svg_inline_text_(layout_svg_inline_text),
      style_(style),
      text_match_color_(text_match_color) {}

const LayoutSVGInlineText& TextPainter::SvgTextPaintState::InlineText() const {
  return layout_svg_inline_text_;
}

const LayoutObject& TextPainter::SvgTextPaintState::TextDecorationObject()
    const {
  // Lookup the first LayoutObject in parent hierarchy which has text-decoration
  // set.
  const LayoutObject* result = InlineText().Parent();
  while (result) {
    if (style_variant_ == StyleVariant::kFirstLine) {
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

const ComputedStyle& TextPainter::SvgTextPaintState::Style() const {
  return style_;
}

bool TextPainter::SvgTextPaintState::IsPaintingSelection() const {
  return is_painting_selection_;
}

PaintFlags TextPainter::SvgTextPaintState::GetPaintFlags() const {
  return paint_flags_;
}

bool TextPainter::SvgTextPaintState::IsRenderingClipPathAsMaskImage() const {
  return paint_flags_ & PaintFlag::kPaintingClipPathAsMask;
}

bool TextPainter::SvgTextPaintState::IsPaintingTextMatch() const {
  return text_match_color_.has_value();
}

Color TextPainter::SvgTextPaintState::TextMatchColor() const {
  return *text_match_color_;
}

AffineTransform& TextPainter::SvgTextPaintState::EnsureShaderTransform() {
  return shader_transform_ ? shader_transform_.value()
                           : shader_transform_.emplace();
}

const AffineTransform* TextPainter::SvgTextPaintState::GetShaderTransform()
    const {
  return base::OptionalToPtr(shader_transform_);
}

}  // namespace blink
