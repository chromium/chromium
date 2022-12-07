// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/highlight_pseudo_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/document_marker_painter.h"
#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/selection_bounds_recorder.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/paint/text_painter.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

class HTMLAnchorElement;

namespace {

// If an inline text box is truncated by an ellipsis, text box markers paint
// over the ellipsis and other marker types don't. Other marker types that want
// the normal behavior should use MarkerPaintStartAndEnd().
std::pair<unsigned, unsigned> GetTextMatchMarkerPaintOffsets(
    const DocumentMarker& marker,
    const InlineTextBox& text_box) {
  // text_box.Start() returns an offset relative to the start of the layout
  // object. We add the LineLayoutItem's TextStartOffset() to get a DOM offset
  // (which is what DocumentMarker uses). This is necessary to get proper
  // behavior with the :first-letter psuedo element.
  const unsigned text_box_start =
      text_box.Start() + text_box.GetLineLayoutItem().TextStartOffset();

  DCHECK(marker.GetType() == DocumentMarker::kTextMatch ||
         marker.GetType() == DocumentMarker::kTextFragment ||
         marker.GetType() == DocumentMarker::kCustomHighlight);
  const unsigned start_offset = marker.StartOffset() > text_box_start
                                    ? marker.StartOffset() - text_box_start
                                    : 0U;
  const unsigned end_offset =
      std::min(marker.EndOffset() - text_box_start, text_box.Len());
  return std::make_pair(start_offset, end_offset);
}

DOMNodeId GetNodeHolder(Node* node) {
  if (node && node->GetLayoutObject())
    return To<LayoutText>(node->GetLayoutObject())->EnsureNodeId();
  return kInvalidDOMNodeId;
}

}  // anonymous namespace

static LineLayoutItem EnclosingUnderlineObject(
    const InlineTextBox* inline_text_box) {
  bool first_line = inline_text_box->IsFirstLineStyle();
  for (LineLayoutItem current = inline_text_box->Parent()->GetLineLayoutItem();
       ;) {
    if (current.IsLayoutBlock())
      return current;
    if (!current.IsLayoutInline() || current.IsRubyText())
      return nullptr;

    const ComputedStyle& style_to_use = current.StyleRef(first_line);
    if (EnumHasFlags(style_to_use.GetTextDecorationLine(),
                     TextDecorationLine::kUnderline))
      return current;

    current = current.Parent();
    if (!current)
      return current;

    if (Node* node = current.GetNode()) {
      if (IsA<HTMLAnchorElement>(node) ||
          node->HasTagName(html_names::kFontTag))
        return current;
    }
  }
}

LayoutObject& InlineTextBoxPainter::InlineLayoutObject() const {
  return *LineLayoutAPIShim::LayoutObjectFrom(
      inline_text_box_.GetLineLayoutItem());
}

static void ComputeOriginAndWidthForBox(const InlineTextBox& box,
                                        PhysicalOffset& local_origin,
                                        LayoutUnit& width) {
  if (box.Truncation() != kCNoTruncation) {
    bool ltr = box.IsLeftToRightDirection();
    bool flow_is_ltr =
        box.GetLineLayoutItem().StyleRef().IsLeftToRightDirection();
    width = LayoutUnit(box.GetLineLayoutItem().Width(
        ltr == flow_is_ltr ? box.Start() : box.Start() + box.Truncation(),
        ltr == flow_is_ltr ? box.Truncation() : box.Len() - box.Truncation(),
        box.TextPos(), flow_is_ltr ? TextDirection::kLtr : TextDirection::kRtl,
        box.IsFirstLineStyle()));
    if (!flow_is_ltr)
      local_origin += PhysicalOffset(box.LogicalWidth() - width, LayoutUnit());
  }
}

void InlineTextBoxPainter::Paint(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset) {
  // We can skip painting if the text box is empty and has no selection.
  if (inline_text_box_.Truncation() == kCFullTruncation ||
      !inline_text_box_.Len())
    return;

  const auto& style_to_use = inline_text_box_.GetLineLayoutItem().StyleRef(
      inline_text_box_.IsFirstLineStyle());
  if (style_to_use.Visibility() != EVisibility::kVisible)
    return;

  DCHECK(!ShouldPaintSelfOutline(paint_info.phase) &&
         !ShouldPaintDescendantOutlines(paint_info.phase));

  bool is_printing =
      inline_text_box_.GetLineLayoutItem().GetDocument().Printing();

  // Determine whether or not we're selected.
  bool have_selection = !is_printing &&
                        paint_info.phase != PaintPhase::kTextClip &&
                        inline_text_box_.IsSelected();
  if (!have_selection && paint_info.phase == PaintPhase::kSelectionDragImage) {
    // When only painting the selection, don't bother to paint if there is none.
    return;
  }

  PhysicalRect physical_overflow = inline_text_box_.PhysicalOverflowRect();
  if (!paint_info.IntersectsCullRect(physical_overflow, paint_offset) &&
      !have_selection)
    return;

  physical_overflow.Move(paint_offset);
  gfx::Rect visual_rect = ToEnclosingRect(physical_overflow);

  if (paint_info.phase == PaintPhase::kForeground) {
    if (auto* mf_checker = MobileFriendlinessChecker::From(
            inline_text_box_.GetLineLayoutItem().GetDocument())) {
      if (const auto* text = DynamicTo<LayoutText>(InlineLayoutObject())) {
        PhysicalRect clipped_rect = PhysicalRect(visual_rect);
        clipped_rect.Intersect(PhysicalRect(paint_info.GetCullRect().Rect()));
        mf_checker->NotifyPaintTextFragment(
            clipped_rect, text->StyleRef().FontSize(),
            paint_info.context.GetPaintController()
                .CurrentPaintChunkProperties()
                .Transform());
      }
    }
  }

  GraphicsContext& context = paint_info.context;
  PhysicalOffset box_origin =
      inline_text_box_.PhysicalLocation() + paint_offset;

  // We round the y-axis to ensure consistent line heights.
  if (inline_text_box_.IsHorizontal()) {
    box_origin.top = LayoutUnit(box_origin.top.Round());
  } else {
    box_origin.left = LayoutUnit(box_origin.left.Round());
  }

  // If vertical, |box_rect| is in the physical coordinates space under the
  // rotation transform.
  PhysicalRect box_rect(box_origin,
                        PhysicalSize(inline_text_box_.LogicalWidth(),
                                     inline_text_box_.LogicalHeight()));

  absl::optional<SelectionBoundsRecorder> selection_recorder;
  // Empty selections might be the boundary of the document selection, and thus
  // need to get recorded.
  const bool should_record_selection =
      have_selection ||
      inline_text_box_.GetLineLayoutItem().GetLayoutObject()->IsSelected();
  if (should_record_selection && paint_info.phase == PaintPhase::kForeground &&
      !is_printing) {
    const FrameSelection& frame_selection =
        InlineLayoutObject().GetFrame()->Selection();
    SelectionState selection_state =
        frame_selection.ComputeLayoutSelectionStateForInlineTextBox(
            inline_text_box_);
    if (SelectionBoundsRecorder::ShouldRecordSelection(frame_selection,
                                                       selection_state)) {
      PhysicalRect selection_rect =
          GetSelectionRect<InlineTextBoxPainter::PaintOptions::kNormal>(
              context, box_rect, style_to_use, style_to_use.GetFont(), nullptr,
              /* allow_empty_selection*/ true);

      TextDirection direction = inline_text_box_.IsLeftToRightDirection()
                                    ? TextDirection::kLtr
                                    : TextDirection::kRtl;
      // We need to account for vertical writing mode rotation - for the
      // actual painting of the selection_rect, this is done below by
      // concatenating a rotation matrix on the context.
      if (!style_to_use.IsHorizontalWritingMode()) {
        gfx::RectF rotated_selection =
            TextPainterBase::Rotation(box_rect, TextPainterBase::kClockwise)
                .MapRect(static_cast<gfx::RectF>(selection_rect));
        selection_rect = PhysicalRect::EnclosingRect(rotated_selection);
      }
      selection_recorder.emplace(
          selection_state, selection_rect, context.GetPaintController(),
          direction, style_to_use.GetWritingMode(), InlineLayoutObject());
    }
  }

  // The text clip phase already has a DrawingRecorder. Text clips are initiated
  // only in BoxPainter::PaintFillLayer, which is already within a
  // DrawingRecorder.
  absl::optional<DrawingRecorder> recorder;
  if (paint_info.phase != PaintPhase::kTextClip) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(context, inline_text_box_,
                                                    paint_info.phase))
      return;
    recorder.emplace(context, inline_text_box_, paint_info.phase, visual_rect);
  }

  unsigned length = inline_text_box_.Len();
  const String& layout_item_string =
      inline_text_box_.GetLineLayoutItem().GetText();

  String first_line_string;
  if (inline_text_box_.IsFirstLineStyle()) {
    first_line_string = layout_item_string;
    const ComputedStyle& style = inline_text_box_.GetLineLayoutItem().StyleRef(
        inline_text_box_.IsFirstLineStyle());
    style.ApplyTextTransform(
        &first_line_string,
        inline_text_box_.GetLineLayoutItem().PreviousCharacter());
    // TODO(crbug.com/795498): this is a hack. The root issue is that
    // capitalizing letters can change the length of the backing string.
    // That needs to be taken into account when computing the size of the box
    // or its painting.
    if (inline_text_box_.Start() >= first_line_string.length())
      return;
    length =
        std::min(length, first_line_string.length() - inline_text_box_.Start());

    // TODO(szager): Figure out why this CHECK sometimes fails, it shouldn't.
    CHECK_LE(inline_text_box_.Start() + length, first_line_string.length());
  } else {
    // TODO(szager): Figure out why this CHECK sometimes fails, it shouldn't.
    CHECK_LE(inline_text_box_.Start() + length, layout_item_string.length());
  }
  StringView string =
      StringView(inline_text_box_.IsFirstLineStyle() ? first_line_string
                                                     : layout_item_string,
                 inline_text_box_.Start(), length);
  int maximum_length = inline_text_box_.GetLineLayoutItem().TextLength() -
                       inline_text_box_.Start();

  StringBuilder characters_with_hyphen;
  TextRun text_run = inline_text_box_.ConstructTextRun(
      style_to_use, string, maximum_length,
      inline_text_box_.HasHyphen() ? &characters_with_hyphen : nullptr);
  if (inline_text_box_.HasHyphen())
    length = text_run.length();

  absl::optional<AffineTransform> rotation;
  absl::optional<GraphicsContextStateSaver> state_saver;
  LayoutTextCombine* combined_text = nullptr;
  if (!inline_text_box_.IsHorizontal()) {
    if (style_to_use.HasTextCombine() &&
        inline_text_box_.GetLineLayoutItem().IsCombineText()) {
      combined_text = &To<LayoutTextCombine>(InlineLayoutObject());
      if (!combined_text->IsCombined())
        combined_text = nullptr;
    }
    if (combined_text) {
      box_rect.SetWidth(combined_text->InlineWidthForLayout());
      // Justfication applies to before and after the combined text as if
      // it is an ideographic character, and is prohibited inside the
      // combined text.
      if (float expansion = text_run.Expansion()) {
        text_run.SetExpansion(0);
        if (text_run.AllowsLeadingExpansion()) {
          if (text_run.AllowsTrailingExpansion())
            expansion /= 2;
          PhysicalOffset offset(LayoutUnit(),
                                LayoutUnit::FromFloatRound(expansion));
          box_origin += offset;
          box_rect.Move(offset);
        }
      }
    } else {
      rotation.emplace(
          TextPainterBase::Rotation(box_rect, TextPainterBase::kClockwise));
      state_saver.emplace(context);
      context.ConcatCTM(*rotation);
    }
  }

  // Determine text colors.
  TextPaintStyle text_style = TextPainterBase::TextPaintingStyle(
      inline_text_box_.GetLineLayoutItem().GetDocument(), style_to_use,
      paint_info);
  TextPaintStyle selection_style = text_style;
  if (have_selection) {
    selection_style = TextPainterBase::SelectionPaintingStyle(
        inline_text_box_.GetLineLayoutItem().GetDocument(), style_to_use,
        inline_text_box_.GetLineLayoutItem().GetNode(), paint_info, text_style);
  }
  bool paint_selected_text_only =
      (paint_info.phase == PaintPhase::kSelectionDragImage);
  bool paint_selected_text_separately =
      !paint_selected_text_only && text_style != selection_style;

  // Set our font.
  const Font& font = style_to_use.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);

  int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  PhysicalOffset text_origin(box_origin.left, box_origin.top + ascent);

  const DocumentMarkerVector& markers_to_paint = ComputeMarkersToPaint();

  // 1. Paint backgrounds behind text if needed. Examples of such backgrounds
  // include selection and composition highlights.
  if (paint_info.phase != PaintPhase::kSelectionDragImage &&
      paint_info.phase != PaintPhase::kTextClip && !is_printing) {
    PaintDocumentMarkers(markers_to_paint, paint_info, box_origin, style_to_use,
                         font, DocumentMarkerPaintPhase::kBackground);
    if (have_selection) {
      PhysicalRect selection_rect;
      if (combined_text) {
        selection_rect =
            PaintSelection<InlineTextBoxPainter::PaintOptions::kCombinedText>(
                context, box_rect, style_to_use, font,
                selection_style.fill_color, combined_text);
      } else {
        selection_rect =
            PaintSelection<InlineTextBoxPainter::PaintOptions::kNormal>(
                context, box_rect, style_to_use, font,
                selection_style.fill_color);
      }

      if (recorder && !box_rect.Contains(selection_rect)) {
        gfx::Rect selection_visual_rect = ToEnclosingRect(selection_rect);
        if (rotation)
          selection_visual_rect = rotation->MapRect(selection_visual_rect);
        recorder->UniteVisualRect(selection_visual_rect);
      }
    }
  }

  // 2. Now paint the foreground, including text and decorations.
  int selection_start = 0;
  int selection_end = 0;
  if (paint_selected_text_only || paint_selected_text_separately)
    inline_text_box_.SelectionStartEnd(selection_start, selection_end);

  bool respect_hyphen =
      selection_end == static_cast<int>(inline_text_box_.Len()) &&
      inline_text_box_.HasHyphen();
  if (respect_hyphen)
    selection_end = text_run.length();

  bool ltr = inline_text_box_.IsLeftToRightDirection();
  bool flow_is_ltr = inline_text_box_.GetLineLayoutItem()
                         .ContainingBlock()
                         .StyleRef()
                         .IsLeftToRightDirection();

  const PaintOffsets& selection_offsets =
      ApplyTruncationToPaintOffsets({static_cast<unsigned>(selection_start),
                                     static_cast<unsigned>(selection_end)});
  selection_start = selection_offsets.start;
  selection_end = selection_offsets.end;
  if (have_selection) {
    font.ExpandRangeToIncludePartialGlyphs(text_run, &selection_start,
                                           &selection_end);
  }

  if (inline_text_box_.Truncation() != kCNoTruncation) {
    // In a mixed-direction flow the ellipsis is at the start of the text
    // rather than at the end of it.
    length =
        ltr == flow_is_ltr ? inline_text_box_.Truncation() : text_run.length();
  }

  TextPainter text_painter(context, font, text_run, text_origin, box_rect,
                           inline_text_box_.IsHorizontal());
  TextEmphasisPosition emphasis_mark_position;
  bool has_text_emphasis = inline_text_box_.GetEmphasisMarkPosition(
      style_to_use, emphasis_mark_position);
  if (has_text_emphasis)
    text_painter.SetEmphasisMark(style_to_use.TextEmphasisMarkString(),
                                 emphasis_mark_position);
  if (combined_text)
    text_painter.SetCombinedText(combined_text);
  if (inline_text_box_.Truncation() != kCNoTruncation && ltr != flow_is_ltr)
    text_painter.SetEllipsisOffset(inline_text_box_.Truncation());

  DOMNodeId node_id = GetNodeHolder(
      LineLayoutAPIShim::LayoutObjectFrom(inline_text_box_.GetLineLayoutItem())
          ->GetNode());
  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      style_to_use, DarkModeFilter::ElementRole::kForeground));

  if (!paint_selected_text_only) {
    // Paint text decorations except line-through.
    absl::optional<TextDecorationInfo> decoration_info;
    if (style_to_use.TextDecorationsInEffect() != TextDecorationLine::kNone &&
        inline_text_box_.Truncation() != kCFullTruncation) {
      PhysicalOffset local_origin = box_origin;
      LayoutUnit width = inline_text_box_.LogicalWidth();
      ComputeOriginAndWidthForBox(inline_text_box_, local_origin, width);
      const LineLayoutItem& decorating_box =
          EnclosingUnderlineObject(&inline_text_box_);
      const ComputedStyle* decorating_box_style =
          decorating_box ? decorating_box.Style() : nullptr;
      absl::optional<AppliedTextDecoration> selection_text_decoration =
          UNLIKELY(have_selection)
              ? absl::optional<AppliedTextDecoration>(
                    selection_style.selection_text_decoration)
              : absl::nullopt;
      decoration_info.emplace(
          local_origin, width, style_to_use,
          /* inline_context */ nullptr, selection_text_decoration,
          /* decoration_override */ nullptr, /* font_override */ nullptr,
          MinimumThickness1(true), 1.0f, inline_text_box_.Root().BaselineType(),
          decorating_box_style);
      TextDecorationOffset decoration_offset(decoration_info->TargetStyle(),
                                             &inline_text_box_, decorating_box);
      text_painter.PaintDecorationsExceptLineThrough(
          decoration_offset, decoration_info.value(), paint_info,
          style_to_use.AppliedTextDecorations(), text_style);
    }

    int start_offset = 0;
    int end_offset = length;
    // Where the text and its flow have opposite directions then our offset into
    // the text given by |truncation| is at the start of the part that will be
    // visible.
    if (inline_text_box_.Truncation() != kCNoTruncation && ltr != flow_is_ltr) {
      start_offset = inline_text_box_.Truncation();
      end_offset = text_run.length();
    }

    if (paint_selected_text_separately && selection_start < selection_end) {
      start_offset = selection_end;
      end_offset = selection_start;
    }
    text_painter.Paint(start_offset, end_offset, length, text_style, node_id,
                       auto_dark_mode);

    // Paint line-through decoration if needed.
    if (decoration_info.has_value()) {
      text_painter.PaintDecorationsOnlyLineThrough(
          decoration_info.value(), paint_info,
          style_to_use.AppliedTextDecorations(), text_style);
    }
  }

  if ((paint_selected_text_only || paint_selected_text_separately) &&
      selection_start < selection_end) {
    // paint only the text that is selected.
    // Because only a part of the text glyph can be selected, we need to draw
    // the selection twice:
    PhysicalRect selection_rect =
        GetSelectionRect<InlineTextBoxPainter::PaintOptions::kNormal>(
            context, box_rect, style_to_use, font);

    // the first time, we draw the glyphs outside the selection area, with
    // the original style.
    {
      GraphicsContextStateSaver inner_state_saver(context);
      context.ClipOut(gfx::RectF(selection_rect));
      text_painter.Paint(selection_start, selection_end, length, text_style,
                         node_id, auto_dark_mode);
    }
    // the second time, we draw the glyphs inside the selection area, with
    // the selection style.
    {
      GraphicsContextStateSaver inner_state_saver(context);
      context.Clip(gfx::RectF(selection_rect));
      text_painter.Paint(selection_start, selection_end, length,
                         selection_style, node_id, auto_dark_mode);
    }
  }

  if (paint_info.phase == PaintPhase::kForeground) {
    PaintDocumentMarkers(markers_to_paint, paint_info, box_origin, style_to_use,
                         font, DocumentMarkerPaintPhase::kForeground);
  }

  if (!font.ShouldSkipDrawing())
    PaintTimingDetector::NotifyTextPaint(visual_rect);
}

InlineTextBoxPainter::PaintOffsets
InlineTextBoxPainter::ApplyTruncationToPaintOffsets(
    const InlineTextBoxPainter::PaintOffsets& offsets) {
  const uint16_t truncation = inline_text_box_.Truncation();
  if (truncation == kCNoTruncation)
    return offsets;

  // If we're in mixed-direction mode (LTR text in an RTL box or vice-versa),
  // the truncation ellipsis is at the *start* of the text box rather than the
  // end.
  bool ltr = inline_text_box_.IsLeftToRightDirection();
  bool flow_is_ltr = inline_text_box_.GetLineLayoutItem()
                         .ContainingBlock()
                         .StyleRef()
                         .IsLeftToRightDirection();

  // truncation is relative to the start of the InlineTextBox, not the text
  // node.
  if (ltr == flow_is_ltr) {
    return {std::min<unsigned>(offsets.start, truncation),
            std::min<unsigned>(offsets.end, truncation)};
  }

  return {std::max<unsigned>(offsets.start, truncation),
          std::max<unsigned>(offsets.end, truncation)};
}

InlineTextBoxPainter::PaintOffsets InlineTextBoxPainter::MarkerPaintStartAndEnd(
    const DocumentMarker& marker) {
  // Text match markers are painted differently (in an inline text box truncated
  // by an ellipsis, they paint over the ellipsis) and so should not use this
  // function.
  DCHECK(marker.GetType() != DocumentMarker::kTextMatch &&
         marker.GetType() != DocumentMarker::kTextFragment);
  DCHECK(inline_text_box_.Truncation() != kCFullTruncation);
  DCHECK(inline_text_box_.Len());

  // inline_text_box_.Start() returns an offset relative to the start of the
  // layout object. We add the LineLayoutItem's TextStartOffset() to get a DOM
  // offset (which is what DocumentMarker uses). This is necessary to get proper
  // behavior with the :first-letter psuedo element.
  const unsigned inline_text_box_start =
      inline_text_box_.Start() +
      inline_text_box_.GetLineLayoutItem().TextStartOffset();

  // Start painting at the beginning of the text or the specified underline
  // start offset, whichever is greater.
  unsigned paint_start = std::max(inline_text_box_start, marker.StartOffset());
  // Cap the maximum paint start to the last character in the text box.
  paint_start = std::min(paint_start, inline_text_box_.end());

  // End painting just past the end of the text or the specified underline end
  // offset, whichever is less.
  unsigned paint_end = std::min(
      inline_text_box_.end() + 1,
      marker.EndOffset());  // end() points at the last char, not past it.

  // paint_start and paint_end are currently relative to the start of the text
  // node. Subtract to make them relative to the start of the InlineTextBox.
  paint_start -= inline_text_box_start;
  paint_end -= inline_text_box_start;

  return ApplyTruncationToPaintOffsets({paint_start, paint_end});
}

void InlineTextBoxPainter::PaintSingleMarkerBackgroundRun(
    GraphicsContext& context,
    const PhysicalOffset& box_origin,
    const ComputedStyle& style,
    const Font& font,
    Color background_color,
    int start_pos,
    int end_pos) {
  if (background_color == Color::kTransparent)
    return;

  int delta_y = (inline_text_box_.GetLineLayoutItem()
                         .StyleRef()
                         .IsFlippedLinesWritingMode()
                     ? inline_text_box_.Root().SelectionBottom() -
                           inline_text_box_.LogicalBottom()
                     : inline_text_box_.LogicalTop() -
                           inline_text_box_.Root().SelectionTop())
                    .ToInt();
  int sel_height = inline_text_box_.Root().SelectionHeight().ToInt();
  gfx::PointF local_origin(box_origin.left.ToFloat(),
                           box_origin.top.ToFloat() - delta_y);
  context.DrawHighlightForText(
      font, inline_text_box_.ConstructTextRun(style), local_origin, sel_height,
      background_color,
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kSelection),
      start_pos, end_pos);
}

DocumentMarkerVector InlineTextBoxPainter::ComputeMarkersToPaint() const {
  Node* const node = inline_text_box_.GetLineLayoutItem().GetNode();
  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return DocumentMarkerVector();

  DocumentMarkerController& document_marker_controller =
      inline_text_box_.GetLineLayoutItem().GetDocument().Markers();
  return document_marker_controller.ComputeMarkersToPaint(*text_node);
}

void InlineTextBoxPainter::PaintDocumentMarkers(
    const DocumentMarkerVector& markers_to_paint,
    const PaintInfo& paint_info,
    const PhysicalOffset& box_origin,
    const ComputedStyle& style,
    const Font& font,
    DocumentMarkerPaintPhase marker_paint_phase) {
  if (!inline_text_box_.GetLineLayoutItem().GetNode())
    return;

  DCHECK(inline_text_box_.Truncation() != kCFullTruncation);
  DCHECK(inline_text_box_.Len());

  DocumentMarkerVector::const_iterator marker_it = markers_to_paint.begin();
  // Give any document markers that touch this run a chance to draw before the
  // text has been drawn.  Note end() points at the last char, not one past it
  // like endOffset and ranges do.
  for (; marker_it != markers_to_paint.end(); ++marker_it) {
    DCHECK(*marker_it);
    const DocumentMarker& marker = **marker_it;

    if (marker.EndOffset() <= inline_text_box_.Start()) {
      // marker is completely before this run.  This might be a marker that sits
      // before the first run we draw, or markers that were within runs we
      // skipped due to truncation.
      continue;
    }
    if (marker.StartOffset() > inline_text_box_.end()) {
      // marker is completely after this run, bail.  A later run will paint it.
      continue;
    }

    // marker intersects this run.  Paint it.
    switch (marker.GetType()) {
      case DocumentMarker::kSpelling:
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground)
          continue;
        inline_text_box_.PaintDocumentMarker(paint_info, box_origin, marker,
                                             style, font, false);
        break;
      case DocumentMarker::kGrammar:
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground)
          continue;
        inline_text_box_.PaintDocumentMarker(paint_info, box_origin, marker,
                                             style, font, true);
        break;
      case DocumentMarker::kCustomHighlight:
      case DocumentMarker::kTextFragment:
      case DocumentMarker::kTextMatch:
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground) {
          inline_text_box_.PaintTextMarkerBackground(paint_info, box_origin,
                                                     marker, style, font);
        } else {
          inline_text_box_.PaintTextMarkerForeground(paint_info, box_origin,
                                                     marker, style, font);
        }
        break;
      case DocumentMarker::kComposition:
      case DocumentMarker::kActiveSuggestion:
      case DocumentMarker::kSuggestion: {
        const auto& styleable_marker = To<StyleableMarker>(marker);
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground) {
          const PaintOffsets marker_offsets =
              MarkerPaintStartAndEnd(styleable_marker);
          PaintSingleMarkerBackgroundRun(
              paint_info.context, box_origin, style, font,
              styleable_marker.BackgroundColor(), marker_offsets.start,
              marker_offsets.end);
        } else if (DocumentMarkerPainter::ShouldPaintMarkerUnderline(
                       styleable_marker)) {
          PaintStyleableMarkerUnderline(paint_info.context, box_origin,
                                        styleable_marker, style, font);
        }
      } break;
      default:
        // Marker is not painted, or painting code has not been added yet
        break;
    }
  }
}

void InlineTextBoxPainter::PaintDocumentMarker(const PaintInfo& paint_info,
                                               const PhysicalOffset& box_origin,
                                               const DocumentMarker& marker,
                                               const ComputedStyle& style,
                                               const Font& font,
                                               bool grammar) {
  if (inline_text_box_.GetLineLayoutItem().GetDocument().Printing())
    return;

  if (inline_text_box_.Truncation() == kCFullTruncation)
    return;

  LayoutUnit start;  // start of line to draw, relative to tx
  LayoutUnit width = inline_text_box_.LogicalWidth();  // how much line to draw

  // Determine whether we need to measure text
  bool marker_spans_whole_box = true;
  if (inline_text_box_.Start() <= marker.StartOffset())
    marker_spans_whole_box = false;
  if ((inline_text_box_.end() + 1) !=
      marker.EndOffset())  // end points at the last char, not past it
    marker_spans_whole_box = false;
  if (inline_text_box_.Truncation() != kCNoTruncation)
    marker_spans_whole_box = false;

  if (!marker_spans_whole_box || grammar) {
    const PaintOffsets& marker_offsets = MarkerPaintStartAndEnd(marker);

    // Calculate start & width
    int delta_y = (inline_text_box_.GetLineLayoutItem()
                           .StyleRef()
                           .IsFlippedLinesWritingMode()
                       ? inline_text_box_.Root().SelectionBottom() -
                             inline_text_box_.LogicalBottom()
                       : inline_text_box_.LogicalTop() -
                             inline_text_box_.Root().SelectionTop())
                      .ToInt();
    int sel_height = inline_text_box_.Root().SelectionHeight().ToInt();
    PhysicalOffset start_point(box_origin.left, box_origin.top - delta_y);
    TextRun run = inline_text_box_.ConstructTextRun(style);

    // FIXME: Convert the document markers to float rects.
    gfx::Rect marker_rect = gfx::ToEnclosingRect(
        font.SelectionRectForText(run, gfx::PointF(start_point), sel_height,
                                  marker_offsets.start, marker_offsets.end));
    start = marker_rect.x() - start_point.left;
    width = LayoutUnit(marker_rect.width());
  }
  DocumentMarkerPainter::PaintDocumentMarker(
      paint_info, box_origin, style, marker.GetType(),
      PhysicalRect(start, LayoutUnit(), width,
                   inline_text_box_.LogicalHeight()));
}

template <InlineTextBoxPainter::PaintOptions options>
PhysicalRect InlineTextBoxPainter::GetSelectionRect(
    GraphicsContext& context,
    const PhysicalRect& box_rect,
    const ComputedStyle& style,
    const Font& font,
    LayoutTextCombine* combined_text,
    bool allow_empty_selection) {
  // See if we have a selection to paint at all.
  int start_pos, end_pos;
  inline_text_box_.SelectionStartEnd(start_pos, end_pos);
  if (start_pos > end_pos)
    return PhysicalRect();
  if (!allow_empty_selection && start_pos == end_pos)
    return PhysicalRect();

  // If the text is truncated, let the thing being painted in the truncation
  // draw its own highlight.
  unsigned start = inline_text_box_.Start();
  int length = inline_text_box_.Len();
  bool ltr = inline_text_box_.IsLeftToRightDirection();
  bool flow_is_ltr = inline_text_box_.GetLineLayoutItem()
                         .ContainingBlock()
                         .StyleRef()
                         .IsLeftToRightDirection();
  if (inline_text_box_.Truncation() != kCNoTruncation) {
    // In a mixed-direction flow the ellipsis is at the start of the text
    // so we need to start after it. Otherwise we just need to make sure
    // the end of the text is where the ellipsis starts.
    if (ltr != flow_is_ltr)
      start_pos = std::max<int>(start_pos, inline_text_box_.Truncation());
    else
      length = inline_text_box_.Truncation();
  }
  StringView string(inline_text_box_.GetLineLayoutItem().GetText(), start,
                    static_cast<unsigned>(length));

  StringBuilder characters_with_hyphen;
  bool respect_hyphen = end_pos == length && inline_text_box_.HasHyphen();
  TextRun text_run = inline_text_box_.ConstructTextRun(
      style, string,
      inline_text_box_.GetLineLayoutItem().TextLength() -
          inline_text_box_.Start(),
      respect_hyphen ? &characters_with_hyphen : nullptr);
  if (respect_hyphen)
    end_pos = text_run.length();

  if (options == InlineTextBoxPainter::PaintOptions::kCombinedText) {
    DCHECK(combined_text);
    // We can't use the height of m_inlineTextBox because LayoutTextCombine's
    // inlineTextBox is horizontal within vertical flow
    combined_text->TransformToInlineCoordinates(context, box_rect, true);
  }

  LayoutUnit selection_bottom = inline_text_box_.Root().SelectionBottom();
  LayoutUnit selection_top = inline_text_box_.Root().SelectionTop();

  int delta_y =
      RoundToInt(inline_text_box_.GetLineLayoutItem()
                         .StyleRef()
                         .IsFlippedLinesWritingMode()
                     ? selection_bottom - inline_text_box_.LogicalBottom()
                     : inline_text_box_.LogicalTop() - selection_top);
  int sel_height = std::max(0, RoundToInt(selection_bottom - selection_top));

  gfx::PointF local_origin(box_rect.X().ToFloat(),
                           (box_rect.Y() - delta_y).ToFloat());
  PhysicalRect selection_rect =
      PhysicalRect::EnclosingRect(font.SelectionRectForText(
          text_run, local_origin, sel_height, start_pos, end_pos));
  // For line breaks, just painting a selection where the line break itself
  // is rendered is sufficient. Don't select it if there's an ellipsis
  // there.
  if (inline_text_box_.HasWrappedSelectionNewline() &&
      inline_text_box_.Truncation() == kCNoTruncation &&
      !inline_text_box_.IsLineBreak())
    ExpandToIncludeNewlineForSelection(selection_rect);

  // Line breaks report themselves as having zero width for layout purposes,
  // and so will end up positioned at (0, 0), even though we paint their
  // selection highlight with character width. For RTL then, we have to
  // explicitly shift the selection rect over to paint in the right location.
  if (!inline_text_box_.IsLeftToRightDirection() &&
      inline_text_box_.IsLineBreak())
    selection_rect.Move(PhysicalOffset(-selection_rect.Width(), LayoutUnit()));
  if (!flow_is_ltr && !ltr && inline_text_box_.Truncation() != kCNoTruncation) {
    selection_rect.Move(
        PhysicalOffset(inline_text_box_.LogicalWidth() - selection_rect.Width(),
                       LayoutUnit()));
  }

  return selection_rect;
}

template <InlineTextBoxPainter::PaintOptions options>
PhysicalRect InlineTextBoxPainter::PaintSelection(
    GraphicsContext& context,
    const PhysicalRect& box_rect,
    const ComputedStyle& style,
    const Font& font,
    Color text_color,
    LayoutTextCombine* combined_text) {
  auto layout_item = inline_text_box_.GetLineLayoutItem();
  Color c = HighlightPaintingUtils::HighlightBackgroundColor(
      layout_item.GetDocument(), layout_item.StyleRef(), layout_item.GetNode(),
      absl::nullopt, kPseudoIdSelection);
  if (!c.Alpha())
    return PhysicalRect();

  PhysicalRect selection_rect =
      GetSelectionRect<options>(context, box_rect, style, font, combined_text);

  // If the text color ends up being the same as the selection background,
  // invert the selection background.
  if (text_color == c) {
    UseCounter::Count(layout_item.GetDocument(),
                      WebFeature::kSelectionBackgroundColorInversion);
    c = Color(0xff - c.Red(), 0xff - c.Green(), 0xff - c.Blue());
  }

  GraphicsContextStateSaver state_saver(context);

  context.FillRect(
      gfx::RectF(selection_rect), c,
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));
  return selection_rect;
}

void InlineTextBoxPainter::ExpandToIncludeNewlineForSelection(
    PhysicalRect& rect) {
  gfx::OutsetsF outsets;
  float space_width = inline_text_box_.NewlineSpaceWidth();
  if (inline_text_box_.IsLeftToRightDirection())
    outsets.set_right(space_width);
  else
    outsets.set_left(space_width);
  rect.Expand(EnclosingLayoutRectOutsets(outsets));
}

void InlineTextBoxPainter::PaintStyleableMarkerUnderline(
    GraphicsContext& context,
    const PhysicalOffset& box_origin,
    const StyleableMarker& marker,
    const ComputedStyle& style,
    const Font& font) {
  if (inline_text_box_.Truncation() == kCFullTruncation)
    return;

  const PaintOffsets marker_offsets = MarkerPaintStartAndEnd(marker);
  const TextRun& run = inline_text_box_.ConstructTextRun(style);
  // Pass 0 for height since we only care about the width
  const gfx::RectF& marker_rect = font.SelectionRectForText(
      run, gfx::PointF(), 0, marker_offsets.start, marker_offsets.end);
  DocumentMarkerPainter::PaintStyleableMarkerUnderline(
      context, box_origin, marker, style,
      inline_text_box_.GetLineLayoutItem().GetDocument(), marker_rect,
      inline_text_box_.LogicalHeight(),
      inline_text_box_.GetLineLayoutItem().GetDocument().InDarkMode());
}

void InlineTextBoxPainter::PaintTextMarkerForeground(
    const PaintInfo& paint_info,
    const PhysicalOffset& box_origin,
    const DocumentMarker& marker,
    const ComputedStyle& style,
    const Font& font) {
  if (marker.GetType() == DocumentMarker::kTextMatch &&
      !InlineLayoutObject()
           .GetFrame()
           ->GetEditor()
           .MarkedTextMatchesAreHighlighted())
    return;

  const auto paint_offsets =
      GetTextMatchMarkerPaintOffsets(marker, inline_text_box_);
  TextRun run = inline_text_box_.ConstructTextRun(style);

  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  const TextPaintStyle text_style =
      DocumentMarkerPainter::ComputeTextPaintStyleFrom(
          inline_text_box_.GetLineLayoutItem().GetDocument(),
          inline_text_box_.GetLineLayoutItem().GetNode(), style, marker,
          paint_info);
  if (text_style.current_color == Color::kTransparent)
    return;

  // If vertical, |box_rect| is in the physical coordinates space under the
  // rotation transform.
  PhysicalRect box_rect(box_origin,
                        PhysicalSize(inline_text_box_.LogicalWidth(),
                                     inline_text_box_.LogicalHeight()));
  PhysicalOffset text_origin(
      box_origin.left, box_origin.top + font_data->GetFontMetrics().Ascent());
  TextPainter text_painter(paint_info.context, font, run, text_origin, box_rect,
                           inline_text_box_.IsHorizontal());

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kForeground));

  text_painter.Paint(paint_offsets.first, paint_offsets.second,
                     inline_text_box_.Len(), text_style, kInvalidDOMNodeId,
                     auto_dark_mode);
}

void InlineTextBoxPainter::PaintTextMarkerBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& box_origin,
    const DocumentMarker& marker,
    const ComputedStyle& style,
    const Font& font) {
  if (marker.GetType() == DocumentMarker::kTextMatch &&
      !LineLayoutAPIShim::LayoutObjectFrom(inline_text_box_.GetLineLayoutItem())
           ->GetFrame()
           ->GetEditor()
           .MarkedTextMatchesAreHighlighted())
    return;

  const auto paint_offsets =
      GetTextMatchMarkerPaintOffsets(marker, inline_text_box_);
  TextRun run = inline_text_box_.ConstructTextRun(style);

  Color color;
  if (marker.GetType() == DocumentMarker::kTextMatch) {
    color = LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
        To<TextMatchMarker>(marker).IsActiveMatch(), style.UsedColorScheme());
  } else {
    DCHECK(marker.GetType() == DocumentMarker::kCustomHighlight ||
           marker.GetType() == DocumentMarker::kTextFragment);
    const auto& highlight_pseudo_marker = To<HighlightPseudoMarker>(marker);
    auto layout_item = inline_text_box_.GetLineLayoutItem();
    color = HighlightPaintingUtils::HighlightBackgroundColor(
        layout_item.GetDocument(), layout_item.StyleRef(),
        layout_item.GetNode(), absl::nullopt,
        highlight_pseudo_marker.GetPseudoId(),
        highlight_pseudo_marker.GetPseudoArgument());
  }
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);

  // If vertical, |box_rect| is in the physical coordinates space under the
  // rotation transform.
  PhysicalRect box_rect(box_origin,
                        PhysicalSize(inline_text_box_.LogicalWidth(),
                                     inline_text_box_.LogicalHeight()));
  context.Clip(gfx::RectF(box_rect));
  context.DrawHighlightForText(
      font, run, gfx::PointF(box_origin), box_rect.Height().ToInt(), color,
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kSelection),
      paint_offsets.first, paint_offsets.second);
}

}  // namespace blink
