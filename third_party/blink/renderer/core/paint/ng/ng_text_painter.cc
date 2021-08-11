// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

class SelectionStyleScope {
  STACK_ALLOCATED();

 public:
  SelectionStyleScope(LayoutObject&,
                      const ComputedStyle& style,
                      const ComputedStyle& selection_style);
  SelectionStyleScope(const SelectionStyleScope&) = delete;
  SelectionStyleScope& operator=(const SelectionStyleScope) = delete;
  ~SelectionStyleScope();

 private:
  LayoutObject& layout_object_;
  const ComputedStyle& selection_style_;
  const bool styles_are_equal_;
};

SelectionStyleScope::SelectionStyleScope(LayoutObject& layout_object,
                                         const ComputedStyle& style,
                                         const ComputedStyle& selection_style)
    : layout_object_(layout_object),
      selection_style_(selection_style),
      styles_are_equal_(style == selection_style) {
  if (styles_are_equal_)
    return;
  DCHECK(!layout_object.IsSVGInlineText());
  auto& element = To<SVGElement>(*layout_object_.GetNode());
  SVGResources::UpdatePaints(element, nullptr, selection_style_);
}

SelectionStyleScope::~SelectionStyleScope() {
  if (styles_are_equal_)
    return;
  auto& element = To<SVGElement>(*layout_object_.GetNode());
  SVGResources::ClearPaints(element, &selection_style_);
}

bool SetupPaintForSvgText(const LayoutSVGInlineText& svg_inline_text,
                          const GraphicsContext& context,
                          bool is_rendering_clip_path_as_mask_image,
                          const ComputedStyle& style,
                          const AffineTransform* shader_transform,
                          LayoutSVGResourceMode resource_mode,
                          PaintFlags& flags) {
  const LayoutObject* layout_parent = svg_inline_text.Parent();
  if (!SVGObjectPainter(*layout_parent)
           .PreparePaint(context, is_rendering_clip_path_as_mask_image, style,
                         resource_mode, flags, shader_transform)) {
    return false;
  }

  flags.setAntiAlias(true);

  if (style.TextShadow() &&
      // Text shadows are disabled when printing. http://crbug.com/258321
      !svg_inline_text.GetDocument().Printing()) {
    flags.setLooper(TextPainterBase::CreateDrawLooper(
        style.TextShadow(), DrawLooperBuilder::kShadowRespectsAlpha,
        style.VisitedDependentColor(GetCSSPropertyColor()),
        style.UsedColorScheme()));
  }

  if (resource_mode == kApplyToStrokeMode) {
    // The stroke geometry needs be generated based on the scaled font.
    float stroke_scale_factor =
        style.VectorEffect() != EVectorEffect::kNonScalingStroke
            ? svg_inline_text.ScalingFactor()
            : 1;
    StrokeData stroke_data;
    SVGLayoutSupport::ApplyStrokeStyleToStrokeData(
        stroke_data, style, *layout_parent, stroke_scale_factor);
    if (stroke_scale_factor != 1)
      stroke_data.SetThickness(stroke_data.Thickness() * stroke_scale_factor);
    stroke_data.SetupPaint(&flags);
  }

  return true;
}

absl::optional<TextDecorationInfo> DecorationsForLayer(
    const NGFragmentItem& text_item,
    const PhysicalRect& decoration_rect,
    const ComputedStyle& style,
    const absl::optional<AppliedTextDecoration>& selection_text_decoration) {
  if (style.TextDecorationsInEffect() == TextDecoration::kNone ||
      // Ellipsis should not have text decorations. This is not defined, but
      // 4 impls do this.
      text_item.IsEllipsis()) {
    return absl::nullopt;
  }
  return TextDecorationInfo(decoration_rect.offset, decoration_rect.Width(),
                            style.GetFontBaseline(), style,
                            selection_text_decoration, nullptr);
}

}  // namespace

void NGTextPainter::Paint(unsigned start_offset,
                          unsigned end_offset,
                          unsigned length,
                          const TextPaintStyle& text_style,
                          DOMNodeId node_id,
                          ShadowMode shadow_mode) {
  GraphicsContextStateSaver state_saver(graphics_context_, false);
  UpdateGraphicsContext(graphics_context_, text_style, horizontal_, state_saver,
                        shadow_mode);
  // TODO(layout-dev): Handle combine text here or elsewhere.
  PaintInternal<kPaintText>(start_offset, end_offset, length, node_id);

  if (!emphasis_mark_.IsEmpty()) {
    if (text_style.emphasis_mark_color != text_style.fill_color)
      graphics_context_.SetFillColor(text_style.emphasis_mark_color);
    PaintInternal<kPaintEmphasisMark>(start_offset, end_offset, length,
                                      node_id);
  }
}

// This function paints text twice with different styles in order to:
// 1. Paint glyphs inside of |selection_rect| using |selection_style|, and
//    outside using |text_style|.
// 2. Paint parts of a ligature glyph.
void NGTextPainter::PaintSelectedText(unsigned start_offset,
                                      unsigned end_offset,
                                      unsigned length,
                                      const TextPaintStyle& text_style,
                                      const TextPaintStyle& selection_style,
                                      const PhysicalRect& selection_rect,
                                      DOMNodeId node_id) {
  if (!fragment_paint_info_.shape_result)
    return;

  // Use fast path if all glyphs fit in |selection_rect|. |visual_rect_| is the
  // ink bounds of all glyphs of this text fragment, including characters before
  // |start_offset| or after |end_offset|. Computing exact bounds is expensive
  // that this code only checks bounds of all glyphs.
  IntRect snapped_selection_rect(PixelSnappedIntRect(selection_rect));
  // Allowing 1px overflow is almost unnoticeable, while it can avoid two-pass
  // painting in most small text.
  snapped_selection_rect.Inflate(1);
  if (snapped_selection_rect.Contains(visual_rect_)) {
    absl::optional<base::AutoReset<bool>> is_painting_selection_reset;
    if (svg_text_paint_state_.has_value()) {
      is_painting_selection_reset.emplace(
          &svg_text_paint_state_->is_painting_selection_, true);
    }
    Paint(start_offset, end_offset, length, selection_style, node_id);
    return;
  }

  // Adjust start/end offset when they are in the middle of a ligature. e.g.,
  // when |start_offset| is between a ligature of "fi", it needs to be adjusted
  // to before "f".
  fragment_paint_info_.shape_result->ExpandRangeToIncludePartialGlyphs(
      &start_offset, &end_offset);

  // Because only a part of the text glyph can be selected, we need to draw
  // the selection twice. First, draw the glyphs outside the selection area,
  // with the original style.
  FloatRect float_selection_rect(selection_rect);
  {
    GraphicsContextStateSaver state_saver(graphics_context_);
    graphics_context_.ClipOut(float_selection_rect);
    Paint(start_offset, end_offset, length, text_style, node_id,
          kTextProperOnly);
  }
  // Then draw the glyphs inside the selection area, with the selection style.
  {
    absl::optional<base::AutoReset<bool>> is_painting_selection_reset;
    if (svg_text_paint_state_.has_value()) {
      is_painting_selection_reset.emplace(
          &svg_text_paint_state_->is_painting_selection_, true);
    }
    GraphicsContextStateSaver state_saver(graphics_context_);
    graphics_context_.Clip(float_selection_rect);
    Paint(start_offset, end_offset, length, selection_style, node_id);
  }
}

// Based on legacy TextPainter.
void NGTextPainter::PaintDecorationsExceptLineThrough(
    const NGFragmentItem& text_item,
    const PaintInfo& paint_info,
    const ComputedStyle& style,
    const TextPaintStyle& text_style,
    const PhysicalRect& decoration_rect,
    const absl::optional<AppliedTextDecoration>& selection_decoration,
    bool* has_line_through_decoration) {
  *has_line_through_decoration = false;

  absl::optional<TextDecorationInfo> decoration_info = DecorationsForLayer(
      text_item, decoration_rect, style, selection_decoration);

  if (!decoration_info) {
    return;
  }

  const NGTextDecorationOffset decoration_offset(decoration_info->Style(),
                                                 text_item.Style(), nullptr);

  TextPainterBase::PaintDecorationsExceptLineThrough(
      decoration_offset, *decoration_info, paint_info,
      style.AppliedTextDecorations(), text_style, has_line_through_decoration);
}

// Based on legacy TextPainter.
void NGTextPainter::PaintDecorationsOnlyLineThrough(
    const NGFragmentItem& text_item,
    const PaintInfo& paint_info,
    const ComputedStyle& style,
    const TextPaintStyle& text_style,
    const PhysicalRect& decoration_rect,
    const absl::optional<AppliedTextDecoration>& selection_decoration) {
  absl::optional<TextDecorationInfo> decoration_info = DecorationsForLayer(
      text_item, decoration_rect, style, selection_decoration);

  DCHECK(decoration_info);

  TextPainterBase::PaintDecorationsOnlyLineThrough(
      *decoration_info, paint_info, style.AppliedTextDecorations(), text_style);
}

template <NGTextPainter::PaintInternalStep step>
void NGTextPainter::PaintInternalFragment(
    unsigned from,
    unsigned to,
    DOMNodeId node_id) {
  DCHECK(from <= fragment_paint_info_.text.length());
  DCHECK(to <= fragment_paint_info_.text.length());

  fragment_paint_info_.from = from;
  fragment_paint_info_.to = to;

  if (step == kPaintEmphasisMark) {
    graphics_context_.DrawEmphasisMarks(
        font_, fragment_paint_info_, emphasis_mark_,
        FloatPoint(text_origin_) + IntSize(0, emphasis_mark_offset_));
  } else {
    DCHECK(step == kPaintText);
    if (svg_text_paint_state_.has_value()) {
      PaintSvgTextFragment(node_id);
    } else {
      graphics_context_.DrawText(font_, fragment_paint_info_,
                                 FloatPoint(text_origin_), node_id);
    }
    // TODO(npm): Check that there are non-whitespace characters. See
    // crbug.com/788444.
    graphics_context_.GetPaintController().SetTextPainted();

    if (!font_.ShouldSkipDrawing())
      PaintTimingDetector::NotifyTextPaint(visual_rect_);
  }
}

template <NGTextPainter::PaintInternalStep Step>
void NGTextPainter::PaintInternal(unsigned start_offset,
                                  unsigned end_offset,
                                  unsigned truncation_point,
                                  DOMNodeId node_id) {
  // TODO(layout-dev): We shouldn't be creating text fragments without text.
  if (!fragment_paint_info_.shape_result)
    return;

  if (start_offset <= end_offset) {
    PaintInternalFragment<Step>(start_offset, end_offset, node_id);
  } else {
    if (end_offset > 0) {
      PaintInternalFragment<Step>(ellipsis_offset_, end_offset, node_id);
    }
    if (start_offset < truncation_point) {
      PaintInternalFragment<Step>(start_offset, truncation_point, node_id);
    }
  }
}

void NGTextPainter::ClipDecorationsStripe(float upper,
                                          float stripe_width,
                                          float dilation) {
  if (fragment_paint_info_.from >= fragment_paint_info_.to ||
      !fragment_paint_info_.shape_result)
    return;

  Vector<Font::TextIntercept> text_intercepts;
  font_.GetTextIntercepts(
      fragment_paint_info_, graphics_context_.DeviceScaleFactor(),
      graphics_context_.FillFlags(),
      std::make_tuple(upper, upper + stripe_width), text_intercepts);

  DecorationsStripeIntercepts(upper, stripe_width, dilation, text_intercepts);
}

void NGTextPainter::PaintSvgTextFragment(DOMNodeId node_id) {
  const NGTextPainter::SvgTextPaintState& state = *svg_text_paint_state_;
  absl::optional<SelectionStyleScope> selection_style_scope;
  bool has_fill = state.Style().HasFill();
  bool has_visible_stroke = state.Style().HasVisibleStroke();
  const ComputedStyle* style_to_paint = &state.Style();
  if (state.IsPaintingSelection()) {
    LayoutObject* layout_parent = state.InlineText().Parent();
    style_to_paint =
        layout_parent->GetCachedPseudoElementStyle(kPseudoIdSelection);
    if (style_to_paint) {
      if (!has_fill)
        has_fill = style_to_paint->HasFill();
      if (!has_visible_stroke)
        has_visible_stroke = style_to_paint->HasVisibleStroke();
    } else {
      style_to_paint = &state.Style();
    }

    selection_style_scope.emplace(*layout_parent, state.Style(),
                                  *style_to_paint);
  }

  if (state.IsRenderingClipPathAsMaskImage()) {
    has_fill = true;
    has_visible_stroke = false;
  }

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
      PaintFlags flags;
      if (SetupPaintForSvgText(state.InlineText(), graphics_context_,
                               state.IsRenderingClipPathAsMaskImage(),
                               *style_to_paint, state.GetShaderTransform(),
                               *resource_mode, flags)) {
        graphics_context_.DrawText(font_, fragment_paint_info_,
                                   FloatPoint(text_origin_), flags, node_id);
      }
    }
  }
}

NGTextPainter::SvgTextPaintState& NGTextPainter::SetSvgState(
    const LayoutSVGInlineText& svg_inline_text,
    const ComputedStyle& style,
    bool is_rendering_clip_path_as_mask_image) {
  return svg_text_paint_state_.emplace(svg_inline_text, style,
                                       is_rendering_clip_path_as_mask_image);
}

NGTextPainter::SvgTextPaintState* NGTextPainter::GetSvgState() {
  return base::OptionalOrNullptr(svg_text_paint_state_);
}

NGTextPainter::SvgTextPaintState::SvgTextPaintState(
    const LayoutSVGInlineText& layout_svg_inline_text,
    const ComputedStyle& style,
    bool is_rendering_clip_path_as_mask_image)
    : layout_svg_inline_text_(layout_svg_inline_text),
      style_(style),
      is_rendering_clip_path_as_mask_image_(
          is_rendering_clip_path_as_mask_image) {}

const LayoutSVGInlineText& NGTextPainter::SvgTextPaintState::InlineText()
    const {
  return layout_svg_inline_text_;
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

AffineTransform& NGTextPainter::SvgTextPaintState::EnsureShaderTransform() {
  return shader_transform_ ? shader_transform_.value()
                           : shader_transform_.emplace();
}

const AffineTransform* NGTextPainter::SvgTextPaintState::GetShaderTransform()
    const {
  return base::OptionalOrNullptr(shader_transform_);
}

}  // namespace blink
