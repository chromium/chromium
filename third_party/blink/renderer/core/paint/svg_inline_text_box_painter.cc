// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_inline_text_box_painter.h"

#include <memory>
#include "base/optional.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/line/inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/selection_painting_utils.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

static inline bool TextShouldBePainted(
    const LayoutSVGInlineText& text_layout_object) {
  // Font::pixelSize(), returns FontDescription::computedPixelSize(), which
  // returns "int(x + 0.5)".  If the absolute font size on screen is below
  // x=0.5, don't render anything.
  return text_layout_object.ScaledFont()
      .GetFontDescription()
      .ComputedPixelSize();
}

bool SVGInlineTextBoxPainter::ShouldPaintSelection(
    const PaintInfo& paint_info) const {
  // Don't paint selections when printing.
  if (paint_info.IsPrinting())
    return false;
  // Don't paint selections when rendering a mask, clip-path (as a mask),
  // pattern or feImage (element reference.)
  if (paint_info.IsRenderingResourceSubtree())
    return false;
  return svg_inline_text_box_.IsSelected();
}

static bool HasShadow(const PaintInfo& paint_info, const ComputedStyle& style) {
  // Text shadows are disabled when printing. http://crbug.com/258321
  return style.TextShadow() && !paint_info.IsPrinting();
}

LayoutObject& SVGInlineTextBoxPainter::InlineLayoutObject() const {
  return *LineLayoutAPIShim::LayoutObjectFrom(
      svg_inline_text_box_.GetLineLayoutItem());
}

LayoutObject& SVGInlineTextBoxPainter::ParentInlineLayoutObject() const {
  return *LineLayoutAPIShim::LayoutObjectFrom(
      svg_inline_text_box_.Parent()->GetLineLayoutItem());
}

LayoutSVGInlineText& SVGInlineTextBoxPainter::InlineText() const {
  return ToLayoutSVGInlineText(InlineLayoutObject());
}

void SVGInlineTextBoxPainter::Paint(const PaintInfo& paint_info,
                                    const LayoutPoint& paint_offset) {
  DCHECK(paint_info.phase == PaintPhase::kForeground ||
         paint_info.phase == PaintPhase::kSelection);
  DCHECK(svg_inline_text_box_.Truncation() == kCNoTruncation);

  if (svg_inline_text_box_.GetLineLayoutItem().StyleRef().Visibility() !=
          EVisibility::kVisible ||
      !svg_inline_text_box_.Len())
    return;

  // We're explicitly not supporting composition & custom underlines and custom
  // highlighters -- unlike InlineTextBox.  If we ever need that for SVG, it's
  // very easy to refactor and reuse the code.

  bool have_selection = ShouldPaintSelection(paint_info);
  if (!have_selection && paint_info.phase == PaintPhase::kSelection)
    return;

  LayoutSVGInlineText& text_layout_object = InlineText();
  if (!TextShouldBePainted(text_layout_object))
    return;

  if (!DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, svg_inline_text_box_, paint_info.phase)) {
    LayoutObject& parent_layout_object = ParentInlineLayoutObject();
    const ComputedStyle& style = parent_layout_object.StyleRef();

    DrawingRecorder recorder(paint_info.context, svg_inline_text_box_,
                             paint_info.phase);
    InlineTextBoxPainter text_painter(svg_inline_text_box_);
    const DocumentMarkerVector& markers_to_paint =
        text_painter.ComputeMarkersToPaint();
    text_painter.PaintDocumentMarkers(
        markers_to_paint, paint_info, paint_offset, style,
        text_layout_object.ScaledFont(), DocumentMarkerPaintPhase::kBackground);

    if (!svg_inline_text_box_.TextFragments().IsEmpty())
      PaintTextFragments(paint_info, parent_layout_object);

    text_painter.PaintDocumentMarkers(
        markers_to_paint, paint_info, paint_offset, style,
        text_layout_object.ScaledFont(), DocumentMarkerPaintPhase::kForeground);
  }
}

void SVGInlineTextBoxPainter::PaintTextFragments(
    const PaintInfo& paint_info,
    LayoutObject& parent_layout_object) {
  const ComputedStyle& style = parent_layout_object.StyleRef();
  const SVGComputedStyle& svg_style = style.SvgStyle();

  bool has_fill = svg_style.HasFill();
  bool has_visible_stroke = svg_style.HasVisibleStroke();

  const ComputedStyle* selection_style = &style;
  bool should_paint_selection = ShouldPaintSelection(paint_info);
  if (should_paint_selection) {
    selection_style =
        parent_layout_object.GetCachedPseudoElementStyle(kPseudoIdSelection);
    if (selection_style) {
      const SVGComputedStyle& svg_selection_style = selection_style->SvgStyle();

      if (!has_fill)
        has_fill = svg_selection_style.HasFill();
      if (!has_visible_stroke)
        has_visible_stroke = svg_selection_style.HasVisibleStroke();
    } else {
      selection_style = &style;
    }
  }

  if (paint_info.IsRenderingClipPathAsMaskImage()) {
    has_fill = true;
    has_visible_stroke = false;
  }

  for (const SVGTextFragment& fragment : svg_inline_text_box_.TextFragments()) {
    GraphicsContextStateSaver state_saver(paint_info.context, false);
    base::Optional<AffineTransform> shader_transform;
    if (fragment.IsTransformed()) {
      state_saver.Save();
      const auto fragment_transform = fragment.BuildFragmentTransform();
      paint_info.context.ConcatCTM(fragment_transform);
      DCHECK(fragment_transform.IsInvertible());
      shader_transform = fragment_transform.Inverse();
    }

    // Spec: All text decorations except line-through should be drawn before the
    // text is filled and stroked; thus, the text is rendered on top of these
    // decorations.
    const Vector<AppliedTextDecoration>& decorations =
        style.AppliedTextDecorations();
    for (const AppliedTextDecoration& decoration : decorations) {
      if (EnumHasFlags(decoration.Lines(), TextDecoration::kUnderline))
        PaintDecoration(paint_info, TextDecoration::kUnderline, fragment);
      if (EnumHasFlags(decoration.Lines(), TextDecoration::kOverline))
        PaintDecoration(paint_info, TextDecoration::kOverline, fragment);
    }

    for (int i = 0; i < 3; i++) {
      switch (svg_style.PaintOrderType(i)) {
        case PT_FILL:
          if (has_fill) {
            PaintText(paint_info, style, *selection_style, fragment,
                      kApplyToFillMode, should_paint_selection,
                      base::OptionalOrNullptr(shader_transform));
          }
          break;
        case PT_STROKE:
          if (has_visible_stroke) {
            PaintText(paint_info, style, *selection_style, fragment,
                      kApplyToStrokeMode, should_paint_selection,
                      base::OptionalOrNullptr(shader_transform));
          }
          break;
        case PT_MARKERS:
          // Markers don't apply to text
          break;
        default:
          NOTREACHED();
          break;
      }
    }

    // Spec: Line-through should be drawn after the text is filled and stroked;
    // thus, the line-through is rendered on top of the text.
    for (const AppliedTextDecoration& decoration : decorations) {
      if (EnumHasFlags(decoration.Lines(), TextDecoration::kLineThrough))
        PaintDecoration(paint_info, TextDecoration::kLineThrough, fragment);
    }
  }
}

void SVGInlineTextBoxPainter::PaintSelectionBackground(
    const PaintInfo& paint_info) {
  if (svg_inline_text_box_.GetLineLayoutItem().StyleRef().Visibility() !=
      EVisibility::kVisible)
    return;

  DCHECK(!paint_info.IsPrinting());

  if (paint_info.phase == PaintPhase::kSelection ||
      !ShouldPaintSelection(paint_info))
    return;

  auto layout_item = svg_inline_text_box_.GetLineLayoutItem();
  Color background_color = SelectionPaintingUtils::SelectionBackgroundColor(
      layout_item.GetDocument(), layout_item.StyleRef(), layout_item.GetNode());
  if (!background_color.Alpha())
    return;

  LayoutSVGInlineText& text_layout_object = InlineText();
  if (!TextShouldBePainted(text_layout_object))
    return;

  const ComputedStyle& style =
      svg_inline_text_box_.Parent()->GetLineLayoutItem().StyleRef();

  int start_position, end_position;
  svg_inline_text_box_.SelectionStartEnd(start_position, end_position);

  const Vector<SVGTextFragmentWithRange> fragment_info_list =
      CollectFragmentsInRange(start_position, end_position);
  for (const SVGTextFragmentWithRange& fragment_with_range :
       fragment_info_list) {
    const SVGTextFragment& fragment = fragment_with_range.fragment;
    GraphicsContextStateSaver state_saver(paint_info.context);
    if (fragment.IsTransformed())
      paint_info.context.ConcatCTM(fragment.BuildFragmentTransform());

    paint_info.context.SetFillColor(background_color);
    paint_info.context.FillRect(
        svg_inline_text_box_.SelectionRectForTextFragment(
            fragment, fragment_with_range.start_position,
            fragment_with_range.end_position, style),
        background_color);
  }
}

static inline LayoutObject* FindLayoutObjectDefininingTextDecoration(
    InlineFlowBox* parent_box) {
  // Lookup first layout object in parent hierarchy which has text-decoration
  // set.
  LayoutObject* layout_object = nullptr;
  while (parent_box) {
    layout_object =
        LineLayoutAPIShim::LayoutObjectFrom(parent_box->GetLineLayoutItem());

    if (layout_object->Style() &&
        layout_object->StyleRef().GetTextDecoration() != TextDecoration::kNone)
      break;

    parent_box = parent_box->Parent();
  }

  DCHECK(layout_object);
  return layout_object;
}

// Offset from the baseline for |decoration|. Positive offsets are above the
// baseline.
static inline float BaselineOffsetForDecoration(TextDecoration decoration,
                                                const FontMetrics& font_metrics,
                                                float thickness) {
  // FIXME: For SVG Fonts we need to use the attributes defined in the
  // <font-face> if specified.
  // Compatible with Batik/Presto.
  if (decoration == TextDecoration::kUnderline)
    return -thickness * 1.5f;
  if (decoration == TextDecoration::kOverline)
    return font_metrics.FloatAscent() - thickness;
  if (decoration == TextDecoration::kLineThrough)
    return font_metrics.FloatAscent() * 3 / 8.0f;

  NOTREACHED();
  return 0.0f;
}

static inline float ThicknessForDecoration(TextDecoration, const Font& font) {
  // FIXME: For SVG Fonts we need to use the attributes defined in the
  // <font-face> if specified.
  // Compatible with Batik/Presto
  return font.GetFontDescription().ComputedSize() / 20.0f;
}

void SVGInlineTextBoxPainter::PaintDecoration(const PaintInfo& paint_info,
                                              TextDecoration decoration,
                                              const SVGTextFragment& fragment) {
  if (svg_inline_text_box_.GetLineLayoutItem()
          .StyleRef()
          .TextDecorationsInEffect() == TextDecoration::kNone)
    return;

  if (fragment.width <= 0)
    return;

  // Find out which style defined the text-decoration, as its fill/stroke
  // properties have to be used for drawing instead of ours.
  LayoutObject* decoration_layout_object =
      FindLayoutObjectDefininingTextDecoration(svg_inline_text_box_.Parent());
  const ComputedStyle& decoration_style = decoration_layout_object->StyleRef();

  if (decoration_style.Visibility() != EVisibility::kVisible)
    return;

  float scaling_factor = 1;
  Font scaled_font;
  LayoutSVGInlineText::ComputeNewScaledFontForStyle(
      *decoration_layout_object, scaling_factor, scaled_font);
  DCHECK(scaling_factor);

  float thickness = ThicknessForDecoration(decoration, scaled_font);
  if (thickness <= 0)
    return;

  const SimpleFontData* font_data = scaled_font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  float decoration_offset = BaselineOffsetForDecoration(
      decoration, font_data->GetFontMetrics(), thickness);
  FloatPoint decoration_origin(fragment.x,
                               fragment.y - decoration_offset / scaling_factor);

  Path path;
  path.AddRect(
      FloatRect(decoration_origin,
                FloatSize(fragment.width, thickness / scaling_factor)));

  const SVGComputedStyle& svg_decoration_style = decoration_style.SvgStyle();

  for (int i = 0; i < 3; i++) {
    switch (svg_decoration_style.PaintOrderType(i)) {
      case PT_FILL:
        if (svg_decoration_style.HasFill()) {
          PaintFlags fill_flags;
          if (!SVGObjectPainter(*decoration_layout_object)
                   .PreparePaint(paint_info, decoration_style, kApplyToFillMode,
                                 fill_flags))
            break;
          fill_flags.setAntiAlias(true);
          paint_info.context.DrawPath(path.GetSkPath(), fill_flags);
        }
        break;
      case PT_STROKE:
        if (svg_decoration_style.HasVisibleStroke()) {
          PaintFlags stroke_flags;
          if (!SVGObjectPainter(*decoration_layout_object)
                   .PreparePaint(paint_info, decoration_style,
                                 kApplyToStrokeMode, stroke_flags))
            break;
          stroke_flags.setAntiAlias(true);
          float stroke_scale_factor =
              svg_decoration_style.VectorEffect() == VE_NON_SCALING_STROKE
                  ? 1 / scaling_factor
                  : 1;
          StrokeData stroke_data;
          SVGLayoutSupport::ApplyStrokeStyleToStrokeData(
              stroke_data, decoration_style, *decoration_layout_object,
              stroke_scale_factor);
          if (stroke_scale_factor != 1)
            stroke_data.SetThickness(stroke_data.Thickness() *
                                     stroke_scale_factor);
          stroke_data.SetupPaint(&stroke_flags);
          paint_info.context.DrawPath(path.GetSkPath(), stroke_flags);
        }
        break;
      case PT_MARKERS:
        break;
      default:
        NOTREACHED();
    }
  }
}

bool SVGInlineTextBoxPainter::SetupTextPaint(
    const PaintInfo& paint_info,
    const ComputedStyle& style,
    LayoutSVGResourceMode resource_mode,
    PaintFlags& flags,
    const AffineTransform* shader_transform) {
  LayoutSVGInlineText& text_layout_object = InlineText();

  float scaling_factor = text_layout_object.ScalingFactor();
  DCHECK(scaling_factor);

  base::Optional<AffineTransform> paint_server_transform;

  if (scaling_factor != 1 || shader_transform) {
    paint_server_transform.emplace();

    // Adjust the paint-server coordinate space.
    paint_server_transform->Scale(scaling_factor);

    if (shader_transform)
      paint_server_transform->Multiply(*shader_transform);
  }

  if (!SVGObjectPainter(ParentInlineLayoutObject())
           .PreparePaint(paint_info, style, resource_mode, flags,
                         base::OptionalOrNullptr(paint_server_transform)))
    return false;
  flags.setAntiAlias(true);

  if (HasShadow(paint_info, style)) {
    flags.setLooper(style.TextShadow()->CreateDrawLooper(
        DrawLooperBuilder::kShadowRespectsAlpha,
        style.VisitedDependentColor(GetCSSPropertyColor())));
  }

  if (resource_mode == kApplyToStrokeMode) {
    // The stroke geometry needs be generated based on the scaled font.
    float stroke_scale_factor =
        style.SvgStyle().VectorEffect() != VE_NON_SCALING_STROKE
            ? scaling_factor
            : 1;
    StrokeData stroke_data;
    SVGLayoutSupport::ApplyStrokeStyleToStrokeData(
        stroke_data, style, ParentInlineLayoutObject(), stroke_scale_factor);
    if (stroke_scale_factor != 1)
      stroke_data.SetThickness(stroke_data.Thickness() * stroke_scale_factor);
    stroke_data.SetupPaint(&flags);
  }
  return true;
}

void SVGInlineTextBoxPainter::PaintText(const PaintInfo& paint_info,
                                        TextRun& text_run,
                                        const SVGTextFragment& fragment,
                                        int start_position,
                                        int end_position,
                                        const PaintFlags& flags) {
  LayoutSVGInlineText& text_layout_object = InlineText();
  const Font& scaled_font = text_layout_object.ScaledFont();

  float scaling_factor = text_layout_object.ScalingFactor();
  DCHECK(scaling_factor);

  FloatPoint text_origin(fragment.x, fragment.y);

  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context, false);
  if (scaling_factor != 1) {
    text_origin.Scale(scaling_factor, scaling_factor);
    state_saver.Save();
    context.Scale(1 / scaling_factor, 1 / scaling_factor);
  }

  TextRunPaintInfo text_run_paint_info(text_run);
  text_run_paint_info.from = start_position;
  text_run_paint_info.to = end_position;

  context.DrawText(scaled_font, text_run_paint_info, text_origin, flags,
                   text_layout_object.EnsureNodeId());
  // TODO(npm): Check that there are non-whitespace characters. See
  // crbug.com/788444.
  context.GetPaintController().SetTextPainted();

  if (!scaled_font.ShouldSkipDrawing()) {
    PaintTimingDetector::NotifyTextPaint(
        InlineLayoutObject().FragmentsVisualRectBoundingBox());
  }
}

void SVGInlineTextBoxPainter::PaintText(
    const PaintInfo& paint_info,
    const ComputedStyle& style,
    const ComputedStyle& selection_style,
    const SVGTextFragment& fragment,
    LayoutSVGResourceMode resource_mode,
    bool should_paint_selection,
    const AffineTransform* shader_transform) {
  int start_position = 0;
  int end_position = 0;
  if (should_paint_selection) {
    svg_inline_text_box_.SelectionStartEnd(start_position, end_position);
    should_paint_selection =
        svg_inline_text_box_.MapStartEndPositionsIntoFragmentCoordinates(
            fragment, start_position, end_position);
  }

  // Fast path if there is no selection, just draw the whole chunk part using
  // the regular style.
  TextRun text_run = svg_inline_text_box_.ConstructTextRun(style, fragment);
  if (!should_paint_selection || start_position >= end_position) {
    PaintFlags flags;
    if (SetupTextPaint(paint_info, style, resource_mode, flags,
                       shader_transform))
      PaintText(paint_info, text_run, fragment, 0, fragment.length, flags);
    return;
  }

  // Eventually draw text using regular style until the start position of the
  // selection.
  bool paint_selected_text_only = paint_info.phase == PaintPhase::kSelection;
  if (start_position > 0 && !paint_selected_text_only) {
    PaintFlags flags;
    if (SetupTextPaint(paint_info, style, resource_mode, flags,
                       shader_transform))
      PaintText(paint_info, text_run, fragment, 0, start_position, flags);
  }

  // Draw text using selection style from the start to the end position of the
  // selection.
  {
    SVGResourcesCache::TemporaryStyleScope scope(ParentInlineLayoutObject(),
                                                 style, selection_style);

    PaintFlags flags;
    if (SetupTextPaint(paint_info, selection_style, resource_mode, flags,
                       shader_transform)) {
      PaintText(paint_info, text_run, fragment, start_position, end_position,
                flags);
    }
  }

  // Eventually draw text using regular style from the end position of the
  // selection to the end of the current chunk part.
  if (end_position < static_cast<int>(fragment.length) &&
      !paint_selected_text_only) {
    PaintFlags flags;
    if (SetupTextPaint(paint_info, style, resource_mode, flags,
                       shader_transform)) {
      PaintText(paint_info, text_run, fragment, end_position, fragment.length,
                flags);
    }
  }
}

Vector<SVGTextFragmentWithRange> SVGInlineTextBoxPainter::CollectTextMatches(
    const DocumentMarker& marker) const {
  const Vector<SVGTextFragmentWithRange> empty_text_match_list;

  // SVG does not support grammar or spellcheck markers, so skip anything but
  // TextMarkerBase types.
  if (marker.GetType() != DocumentMarker::kTextMatch &&
      marker.GetType() != DocumentMarker::kTextFragment)
    return empty_text_match_list;

  if (marker.GetType() == DocumentMarker::kTextMatch &&
      !InlineLayoutObject()
           .GetFrame()
           ->GetEditor()
           .MarkedTextMatchesAreHighlighted())
    return empty_text_match_list;

  int marker_start_position =
      std::max<int>(marker.StartOffset() - svg_inline_text_box_.Start(), 0);
  int marker_end_position =
      std::min<int>(marker.EndOffset() - svg_inline_text_box_.Start(),
                    svg_inline_text_box_.Len());

  if (marker_start_position >= marker_end_position)
    return empty_text_match_list;

  return CollectFragmentsInRange(marker_start_position, marker_end_position);
}

Vector<SVGTextFragmentWithRange>
SVGInlineTextBoxPainter::CollectFragmentsInRange(int start_position,
                                                 int end_position) const {
  Vector<SVGTextFragmentWithRange> fragment_info_list;
  for (const SVGTextFragment& fragment : svg_inline_text_box_.TextFragments()) {
    // TODO(ramya.v): If these can't be negative we should use unsigned.
    int fragment_start_position = start_position;
    int fragment_end_position = end_position;
    if (!svg_inline_text_box_.MapStartEndPositionsIntoFragmentCoordinates(
            fragment, fragment_start_position, fragment_end_position))
      continue;

    fragment_info_list.push_back(SVGTextFragmentWithRange(
        fragment, fragment_start_position, fragment_end_position));
  }
  return fragment_info_list;
}

void SVGInlineTextBoxPainter::PaintTextMarkerForeground(
    const PaintInfo& paint_info,
    const LayoutPoint& point,
    const TextMarkerBase& marker,
    const ComputedStyle& style,
    const Font& font) {
  const Vector<SVGTextFragmentWithRange> text_match_info_list =
      CollectTextMatches(marker);
  if (text_match_info_list.IsEmpty())
    return;

  Color text_color = LayoutTheme::GetTheme().PlatformTextSearchColor(
      marker.IsActiveMatch(),
      svg_inline_text_box_.GetLineLayoutItem()
          .GetDocument()
          .InForcedColorsMode(),
      style.UsedColorScheme());

  PaintFlags fill_flags;
  fill_flags.setColor(text_color.Rgb());
  fill_flags.setAntiAlias(true);

  PaintFlags stroke_flags;
  bool should_paint_stroke = false;
  if (SetupTextPaint(paint_info, style, kApplyToStrokeMode, stroke_flags,
                     nullptr)) {
    should_paint_stroke = true;
    stroke_flags.setLooper(nullptr);
    stroke_flags.setColor(text_color.Rgb());
  }

  for (const SVGTextFragmentWithRange& text_match_info : text_match_info_list) {
    const SVGTextFragment& fragment = text_match_info.fragment;
    GraphicsContextStateSaver state_saver(paint_info.context);
    if (fragment.IsTransformed())
      paint_info.context.ConcatCTM(fragment.BuildFragmentTransform());

    TextRun text_run = svg_inline_text_box_.ConstructTextRun(style, fragment);
    PaintText(paint_info, text_run, fragment, text_match_info.start_position,
              text_match_info.end_position, fill_flags);
    if (should_paint_stroke) {
      PaintText(paint_info, text_run, fragment, text_match_info.start_position,
                text_match_info.end_position, stroke_flags);
    }
  }
}

void SVGInlineTextBoxPainter::PaintTextMarkerBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& point,
    const TextMarkerBase& marker,
    const ComputedStyle& style,
    const Font& font) {
  const Vector<SVGTextFragmentWithRange> text_match_info_list =
      CollectTextMatches(marker);
  if (text_match_info_list.IsEmpty())
    return;

  Color color = LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
      marker.IsActiveMatch(),
      svg_inline_text_box_.GetLineLayoutItem()
          .GetDocument()
          .InForcedColorsMode(),
      style.UsedColorScheme());
  for (const SVGTextFragmentWithRange& text_match_info : text_match_info_list) {
    const SVGTextFragment& fragment = text_match_info.fragment;

    GraphicsContextStateSaver state_saver(paint_info.context, false);
    if (fragment.IsTransformed()) {
      state_saver.Save();
      paint_info.context.ConcatCTM(fragment.BuildFragmentTransform());
    }
    FloatRect fragment_rect = svg_inline_text_box_.SelectionRectForTextFragment(
        fragment, text_match_info.start_position, text_match_info.end_position,
        style);
    paint_info.context.SetFillColor(color);
    paint_info.context.FillRect(fragment_rect);
  }
}

}  // namespace blink
