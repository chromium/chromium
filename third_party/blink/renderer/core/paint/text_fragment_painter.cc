// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_fragment_painter.h"

#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/inline/fit_text_scale.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/highlight_painter.h"
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/line_relative_rect.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/selection_bounds_recorder.h"
#include "third_party/blink/renderer/core/paint/text_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/text_painter.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

inline const DisplayItemClient& AsDisplayItemClient(const InlineCursor& cursor,
                                                    bool for_selection) {
  if (for_selection) [[unlikely]] {
    if (const auto* selection_client =
            cursor.Current().GetSelectionDisplayItemClient())
      return *selection_client;
  }
  return *cursor.Current().GetDisplayItemClient();
}

inline PhysicalRect PhysicalBoxRect(const InlineCursor& cursor,
                                    const PhysicalOffset& paint_offset,
                                    const PhysicalOffset& parent_offset,
                                    const LayoutTextCombine* text_combine) {
  PhysicalRect box_rect;
  if (const auto* svg_data = cursor.CurrentItem()->GetSvgFragmentData()) {
    box_rect = PhysicalRect::FastAndLossyFromRectF(svg_data->rect);
    const float scale = svg_data->length_adjust_scale;
    if (scale != 1.0f) {
      if (cursor.CurrentItem()->IsHorizontal())
        box_rect.SetWidth(LayoutUnit(svg_data->rect.width() / scale));
      else
        box_rect.SetHeight(LayoutUnit(svg_data->rect.height() / scale));
    }
  } else {
    box_rect = cursor.CurrentItem()->RectInContainerFragment();
  }
  box_rect.offset.left += paint_offset.left;
  // We round the y-axis to ensure consistent line heights.
  box_rect.offset.top =
      LayoutUnit((paint_offset.top + parent_offset.top).Round()) +
      (box_rect.offset.top - parent_offset.top);
  if (text_combine) {
    box_rect.offset.left =
        text_combine->AdjustTextLeftForPaint(box_rect.offset.left);
  }
  return box_rect;
}

inline const InlineCursor& InlineCursorForBlockFlow(
    const InlineCursor& cursor,
    std::optional<InlineCursor>* storage) {
  if (*storage)
    return **storage;
  *storage = cursor;
  (*storage)->ExpandRootToContainingBlock();
  return **storage;
}

bool ShouldPaintEmphasisMark(const ComputedStyle& style,
                             const LayoutObject& layout_object,
                             const FragmentItem& text_item) {
  if (style.GetTextEmphasisMark() == TextEmphasisMark::kNone)
    return false;
  // Note: We set text-emphasis-style:none for combined text and we paint
  // emphasis mark at left/right side of |LayoutTextCombine|.
  DCHECK(!IsA<LayoutTextCombine>(layout_object.Parent()));

  if (text_item.IsEllipsis()) {
    return false;
  }

  if (RuntimeEnabledFeatures::TextEmphasisWithRubyEnabled()) {
    return true;
  }

  if (style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver) {
    return !text_item.HasOverAnnotation();
  }
  return !text_item.HasUnderAnnotation();
}

bool IsDecorationSkipSpace(UChar c) {
  if (c <= 0x7F) {
    return IsAsciiSpace(c);
  }
  return !!(unicode::Category(c) & unicode::kSeparator_Space);
}

bool IsNotDecorationSkipSpace(UChar c) {
  return !IsDecorationSkipSpace(c);
}

struct SpaceSkipWidths {
  LayoutUnit start_width;
  LayoutUnit end_width;
};

Vector<gfx::RectF> ComputeInteriorSpaceRects(const FragmentItem& text_item,
                                             const StringView& item_text,
                                             LayoutUnit decoration_line_over,
                                             LayoutUnit decoration_block_size) {
  Vector<gfx::RectF> rects;
  if (item_text.empty()) {
    return rects;
  }

  const string_size_t len = item_text.length();
  const string_size_t item_start = text_item.StartOffset();
  string_size_t last_non_space =
      item_text.ReverseFind(IsNotDecorationSkipSpace);

  // Skip leading spaces.
  string_size_t i = item_text.Find(IsNotDecorationSkipSpace);
  // Walk through the non-space portions; collect space runs encountered between
  // two non-space characters.
  while (i < len) {
    // Advance past non-space characters.
    i = item_text.Find(IsDecorationSkipSpace, i);
    if (i >= len) {
      break;
    }

    // Found a space run starting at i. Find its end.
    string_size_t space_start = i;
    i = item_text.Find(IsNotDecorationSkipSpace, i);
    string_size_t space_end = i;

    // If this space run starts after the last non-space character, it is a
    // trailing run — stop (handled separately via SpaceSkipWidths).
    if (space_start > last_non_space) {
      break;
    }

    // This is an interior space run.
    auto [left, right] = text_item.LineLeftAndRightForOffsets(
        item_text, item_start + space_start, item_start + space_end);
    if (right > left) {
      rects.emplace_back(left, decoration_line_over, right - left,
                         decoration_block_size);
    }
  }

  return rects;
}

SpaceSkipWidths ComputeSpaceSkipWidths(const FragmentItem& text_item,
                                       const StringView& item_text,
                                       TextDecorationSkipSpaces skip,
                                       bool check_start,
                                       bool check_end) {
  SpaceSkipWidths result;
  if (item_text.empty()) {
    return result;
  }

  const string_size_t item_start = text_item.StartOffset();
  const string_size_t item_end = text_item.EndOffset();
  if (check_start && EnumHasFlags(skip, TextDecorationSkipSpaces::kStart)) {
    // Find how many leading spaces are in this item.
    string_size_t i = item_text.Find(IsNotDecorationSkipSpace);
    i = (i == kNotFound) ? item_text.length() : i;
    if (i > 0) {
      auto [left, right] = text_item.LineLeftAndRightForOffsets(
          item_text, item_start, item_start + i);
      result.start_width = right - left;
    }
  }

  if (check_end && EnumHasFlags(skip, TextDecorationSkipSpaces::kEnd)) {
    // Find how many trailing spaces are in this item.
    const string_size_t len = item_text.length();
    string_size_t pos = item_text.ReverseFind(IsNotDecorationSkipSpace);
    string_size_t i = (pos == kNotFound) ? 0 : pos + 1;
    if (i < len) {
      auto [left, right] = text_item.LineLeftAndRightForOffsets(
          item_text, item_start + i, item_end);
      result.end_width = right - left;
    }
  }

  return result;
}

PhysicalDirection GetDisclosureOrientation(const ComputedStyle& style,
                                           bool is_open) {
  const auto direction_mode = style.GetWritingDirection();
  return is_open ? direction_mode.BlockEnd() : direction_mode.InlineEnd();
}

base::span<const gfx::PointF, 3> GetDisclosurePathPoints(
    PhysicalDirection direction) {
  static constexpr gfx::PointF kLeftPoints[3] = {
      {1.0f, 0.0f}, {0.14f, 0.5f}, {1.0f, 1.0f}};
  static constexpr gfx::PointF kRightPoints[3] = {
      {0.0f, 0.0f}, {0.86f, 0.5f}, {0.0f, 1.0f}};
  static constexpr gfx::PointF kUpPoints[3] = {
      {0.0f, 0.93f}, {0.5f, 0.07f}, {1.0f, 0.93f}};
  static constexpr gfx::PointF kDownPoints[3] = {
      {0.0f, 0.07f}, {0.5f, 0.93f}, {1.0f, 0.07f}};

  switch (direction) {
    case PhysicalDirection::kLeft:
      return kLeftPoints;
    case PhysicalDirection::kRight:
      return kRightPoints;
    case PhysicalDirection::kUp:
      return kUpPoints;
    case PhysicalDirection::kDown:
      return kDownPoints;
  }
}

Path GetCanonicalDisclosurePath(const ComputedStyle& style,
                                const gfx::RectF& bounding_box,
                                bool is_open) {
  auto points =
      GetDisclosurePathPoints(GetDisclosureOrientation(style, is_open));
  auto map_unit_point = [&bounding_box](const gfx::PointF& p) {
    return gfx::ScalePoint(p, bounding_box.size().width(),
                           bounding_box.size().height()) +
           bounding_box.OffsetFromOrigin();
  };

  PathBuilder result;
  result.MoveTo(map_unit_point(points[0]));
  for (size_t i = 1; i < points.size(); ++i) {
    result.LineTo(map_unit_point(points[i]));
  }
  result.Close();
  return result.Finalize();
}

}  // namespace

void TextFragmentPainter::PaintSymbol(const LayoutObject* layout_object,
                                      const ComputedStyle& style,
                                      const PhysicalSize box_size,
                                      const PaintInfo& paint_info,
                                      const PhysicalOffset& paint_offset) {
  const AtomicString& type = LayoutCounter::ListStyle(layout_object, style);
  PhysicalRect marker_rect(
      ListMarker::RelativeSymbolMarkerRect(style, type, box_size.width));
  marker_rect.Move(paint_offset);

  DCHECK(layout_object);
#if DCHECK_IS_ON()
  if (layout_object->IsCounter()) {
    DCHECK(To<LayoutCounter>(layout_object)->IsDirectionalSymbolMarker());
  } else {
    DCHECK(style.ListStyleType());
    DCHECK(style.ListStyleType()->IsCounterStyle());
  }
#endif
  GraphicsContext& context = paint_info.context;
  Color color(layout_object->ResolveColor(GetCSSPropertyColor()));
  if (BoxModelObjectPainter::ShouldForceWhiteBackgroundForPrintEconomy(
          layout_object->GetDocument(), style)) {
    color = TextPainter::TextColorForWhiteBackground(color);
  }
  // Apply the color to the list marker text.
  context.SetFillColor(color);
  context.SetStrokeColor(color);
  const gfx::Rect snapped_rect = ToPixelSnappedRect(marker_rect);
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kListSymbol));
  if (type == keywords::kDisc) {
    context.FillEllipse(gfx::RectF(snapped_rect), auto_dark_mode);
  } else if (type == keywords::kCircle) {
    context.SetStrokeThickness(1.0f);
    context.StrokeEllipse(gfx::RectF(snapped_rect), auto_dark_mode);
  } else if (type == keywords::kSquare) {
    context.FillRect(snapped_rect, color, auto_dark_mode);
  } else if (type == keywords::kDisclosureOpen ||
             type == keywords::kDisclosureClosed) {
    const Path path = GetCanonicalDisclosurePath(
        style, gfx::RectF(marker_rect), type == keywords::kDisclosureOpen);
    context.FillPath(path, auto_dark_mode);
  } else {
    NOTREACHED();
  }
}

void TextFragmentPainter::Paint(const PaintInfo& paint_info,
                                const PhysicalOffset& paint_offset) {
  const auto& text_item = *cursor_.CurrentItem();
  // We can skip painting if the fragment (including selection) is invisible.
  if (!text_item.TextLength())
    return;

  if (!text_item.TextShapeResult() &&
      // A line break's selection tint is still visible.
      !text_item.IsLineBreak())
    return;

  const ComputedStyle& style = text_item.Style();
  if (style.Visibility() != EVisibility::kVisible) {
    return;
  }

  const TextFragmentPaintInfo& fragment_paint_info =
      cursor_.Current()->TextPaintInfo(cursor_.Items());
  const LayoutObject* layout_object = text_item.GetLayoutObject();
  const Document& document = layout_object->GetDocument();
  const bool is_printing = document.Printing();
  // Don't paint selections when rendering a mask, clip-path (as a mask),
  // pattern or feImage (element reference.)
  const bool is_rendering_resource = paint_info.IsRenderingResourceSubtree();
  const auto* const text_combine =
      DynamicTo<LayoutTextCombine>(layout_object->Parent());
  const PhysicalRect physical_box =
      PhysicalBoxRect(cursor_, paint_offset, parent_offset_, text_combine);
#if DCHECK_IS_ON()
  if (text_combine) [[unlikely]] {
    LayoutTextCombine::AssertStyleIsValid(style);
  }
#endif

  if (paint_info.phase == PaintPhase::kForeground) {
    ObjectPainter object_painter(*layout_object);
    if (object_painter.ShouldRecordSpecialHitTestData(paint_info)) {
      object_painter.RecordHitTestData(paint_info,
                                       ToPixelSnappedRect(physical_box),
                                       *text_item.GetDisplayItemClient());
    }
  }

  // Determine whether or not we’ll need a writing-mode rotation, but don’t
  // actually rotate until we reach the steps that need it.
  std::optional<AffineTransform> rotation;
  const WritingMode writing_mode = style.GetWritingMode();
  const bool is_horizontal = IsHorizontalWritingMode(writing_mode);
  const LineRelativeRect rotated_box =
      LineRelativeRect::CreateFromLineBox(physical_box, is_horizontal);
  if (!is_horizontal) {
    rotation.emplace(
        rotated_box.ComputeRelativeToPhysicalTransform(writing_mode));
  }

  // Determine whether or not we're selected.
  HighlightPainter::SelectionPaintState* selection = nullptr;
  std::optional<HighlightPainter::SelectionPaintState>
      selection_for_bounds_recording;
  if (!is_printing && !is_rendering_resource &&
      paint_info.phase != PaintPhase::kTextClip && layout_object->IsSelected())
      [[unlikely]] {
    const InlineCursor& root_inline_cursor =
        InlineCursorForBlockFlow(cursor_, &inline_cursor_for_block_flow_);

    // Empty selections might be the boundary of the document selection, and
    // thus need to get recorded. We only need to paint the selection if it
    // has a valid range.
    selection_for_bounds_recording.emplace(root_inline_cursor,
                                           physical_box.offset, rotation);
    if (selection_for_bounds_recording->Status().HasValidRange())
      selection = &selection_for_bounds_recording.value();
  }
  if (!selection) {
    // When only painting the selection drag image, don't bother to paint if
    // there is none.
    if (paint_info.phase == PaintPhase::kSelectionDragImage)
      return;

    // Flow controls (line break, tab, <wbr>) need only selection painting.
    if (text_item.IsFlowControl())
      return;
  }

  gfx::Rect visual_rect;
  const auto* const svg_inline_text =
      DynamicTo<LayoutSVGInlineText>(layout_object);
  float scaling_factor = 1.0f;
  if (svg_inline_text) [[unlikely]] {
    DCHECK(text_item.IsSvgText());
    scaling_factor = svg_inline_text->ScalingFactor();
    DCHECK_NE(scaling_factor, 0.0f);
    visual_rect = gfx::ToEnclosingRect(
        svg_inline_text->Parent()->VisualRectInLocalSVGCoordinates());
  } else {
    DCHECK(!text_item.IsSvgText());
    if (RuntimeEnabledFeatures::CssTextFitEnabled()) {
      scaling_factor = text_item.GetFitTextScale();
    }
    PhysicalRect ink_overflow = text_item.SelfInkOverflowRect();
    ink_overflow.Move(physical_box.offset);
    visual_rect = ToEnclosingRect(ink_overflow);

    // Expand |visual_rect| to prevent emphasis mark clipping if emphasis mark
    // and nested ruby annotation exist on the same side.
    bool has_over_text_emphasis =
        style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver;
    if (RuntimeEnabledFeatures::TextEmphasisWithRubyEnabled() &&
        ShouldPaintEmphasisMark(style, *layout_object, text_item) &&
        ((has_over_text_emphasis && text_item.HasOverAnnotation()) ||
         (!has_over_text_emphasis && text_item.HasUnderAnnotation()))) {
      gfx::Rect emphasis_rect = visual_rect;
      FontHeight annotation_metrics = text_item.AnnotationMetrics();
      if (has_over_text_emphasis) {
        const auto ascent = annotation_metrics.ascent.Ceil();
        emphasis_rect.set_y(emphasis_rect.y() - ascent);
        emphasis_rect.set_height(emphasis_rect.height() + ascent);
      } else {
        const auto descent = annotation_metrics.descent.Ceil();
        emphasis_rect.set_height(emphasis_rect.height() + descent);
      }
      visual_rect.Union(emphasis_rect);
    }
  }

  // Ensure the selection bounds are recorded on the paint chunk regardless of
  // whether the display item that contains the actual selection painting is
  // reused.
  std::optional<SelectionBoundsRecorder> selection_recorder;
  if (selection_for_bounds_recording &&
      paint_info.phase == PaintPhase::kForeground && !is_printing)
      [[unlikely]] {
    if (SelectionBoundsRecorder::ShouldRecordSelection(
            cursor_.Current().GetLayoutObject()->GetFrame()->Selection(),
            selection_for_bounds_recording->State())) {
      selection_recorder.emplace(
          selection_for_bounds_recording->State(),
          selection_for_bounds_recording->PhysicalSelectionRect(),
          paint_info.context.GetPaintController(),
          cursor_.Current().ResolvedDirection(), style.GetWritingMode());
    }
  }

  // This is declared after selection_recorder so that this will be destructed
  // before selection_recorder to ensure the selection is painted before
  // selection_recorder records the selection bounds.
  std::optional<DrawingRecorder> recorder;
  const auto& display_item_client =
      AsDisplayItemClient(cursor_, selection != nullptr);
  // Text clips are initiated only in BoxPainterBase::PaintFillLayer, which is
  // already within a DrawingRecorder.
  if (paint_info.phase != PaintPhase::kTextClip) {
    if (!paint_info.context.InDrawingRecorder()) [[likely]] {
      if (DrawingRecorder::UseCachedDrawingIfPossible(
              paint_info.context, display_item_client, paint_info.phase)) {
        return;
      }
      recorder.emplace(paint_info.context, display_item_client,
                       paint_info.phase, visual_rect);
    }
  }

  if (text_item.IsSymbolMarker()) [[unlikely]] {
    PaintSymbol(layout_object, style, physical_box.size, paint_info,
                physical_box.offset);
    return;
  }

  GraphicsContext& context = paint_info.context;

  // Determine text colors.

  Node* node = layout_object->GetNode();
  TextPaintStyle text_style =
      TextPainter::TextPaintingStyle(document, style, paint_info);
  if (selection) [[unlikely]] {
    selection->ComputeSelectionStyle(document, style, node, paint_info,
                                     text_style);
  }

  // Set our font.
  const Font* font;
  if (text_combine && text_combine->CompressedFont()) [[unlikely]] {
    font = text_combine->CompressedFont();
  } else {
    font = &text_item.ScaledFont();
  }
  const SimpleFontData* font_data = font->PrimaryFont();
  DCHECK(font_data);

  GraphicsContextStateSaver state_saver(context, /*save_and_restore=*/false);
  const int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  LayoutUnit top = physical_box.offset.top + ascent;
  if (RuntimeEnabledFeatures::CssTextFitEnabled() && !svg_inline_text) {
    top = LayoutUnit(physical_box.offset.top + ascent * scaling_factor);
  }
  LineRelativeOffset text_origin{physical_box.offset.left, top};
  if (text_combine) [[unlikely]] {
    text_origin.line_over =
        text_combine->AdjustTextTopForPaint(physical_box.offset.top);
  }

  TextPainter text_painter(context, paint_info.GetSvgContextPaints(), *font,
                           visual_rect, text_origin);

  // Apply text-decoration-skip-spaces by trimming the decoration box.
  LineRelativeRect decoration_box = rotated_box;
  const TextDecorationSkipSpaces skip_spaces =
      RuntimeEnabledFeatures::CSSTextDecorationSkipSpacesEnabled()
          ? style.GetTextDecorationSkipSpaces()
          : TextDecorationSkipSpaces::kNone;
  // For text-decoration-skip-spaces: all, collect interior space run rects
  // (in item-local line-relative coordinates) so we can clip them out of the
  // decoration canvas after the writing-mode rotation is applied.
  Vector<gfx::RectF> interior_space_rects;
  if (skip_spaces != TextDecorationSkipSpaces::kNone &&
      style.HasAppliedTextDecorations()) {
    const bool is_first_text_on_line = [&]() -> bool {
      if (cursor_.IsAtFirst()) {
        return true;
      }
      // Check if every preceding inline leaf consists entirely of skip spaces.
      InlineCursor prev = cursor_;
      prev.MoveToPreviousInlineLeaf();
      while (prev) {
        const StringView prev_text = prev.CurrentText();
        if (prev_text.IsAllSpecialCharacters<IsDecorationSkipSpace>()) {
          return false;
        }
        prev.MoveToPreviousInlineLeaf();
      }
      return true;
    }();
    const bool is_last_text_on_line = [&]() -> bool {
      InlineCursor next = cursor_;
      next.MoveToNextInlineLeaf();
      while (next) {
        const StringView next_text = next.CurrentText();
        if (!next_text.IsAllSpecialCharacters<IsDecorationSkipSpace>()) {
          return false;
        }
        next.MoveToNextInlineLeaf();
      }
      return true;
    }();

    // For 'all', treat it as 'start end' for space-character trimming, but
    // also trim any trailing letter-spacing at line edges.
    const bool is_all = (skip_spaces == TextDecorationSkipSpaces::kAll);
    const TextDecorationSkipSpaces effective_skip =
        is_all ? (TextDecorationSkipSpaces::kStart |
                  TextDecorationSkipSpaces::kEnd)
               : skip_spaces;

    LayoutUnit extra_end_trim;
    if (is_all && is_last_text_on_line) {
      // 'all' additionally skips letter-spacing adjacent to the line end.
      // letter-spacing is added after each glyph, so the last character on a
      // line has trailing letter-spacing extending the decoration. Trim it.
      const float letter_spacing = style.LetterSpacing();
      if (letter_spacing > 0) {
        extra_end_trim = LayoutUnit(letter_spacing);
      }
    }

    const StringView item_text = cursor_.CurrentText();

    if (is_first_text_on_line || is_last_text_on_line) {
      SpaceSkipWidths skip_widths =
          ComputeSpaceSkipWidths(text_item, item_text, effective_skip,
                                 is_first_text_on_line, is_last_text_on_line);
      skip_widths.end_width += extra_end_trim;
      if (skip_widths.start_width > LayoutUnit() ||
          skip_widths.end_width > LayoutUnit()) {
        decoration_box.offset.line_left += skip_widths.start_width;
        decoration_box.size.inline_size -=
            (skip_widths.start_width + skip_widths.end_width);
      }
    }
    decoration_box.size.inline_size =
        decoration_box.size.inline_size.ClampNegativeToZero();

    if (is_all) {
      interior_space_rects = ComputeInteriorSpaceRects(
          text_item, item_text, decoration_box.offset.line_over,
          decoration_box.size.block_size);
      const LayoutUnit item_line_left = LayoutUnit(physical_box.offset.left);
      for (auto& rect : interior_space_rects) {
        rect.set_x(rect.x() + item_line_left);
      }
    }
  }

  TextDecorationPainter decoration_painter(text_painter, inline_context_,
                                           paint_info, style, text_style,
                                           decoration_box, selection);
  HighlightPainter highlight_painter(
      fragment_paint_info, text_painter, decoration_painter, paint_info,
      cursor_, text_item, physical_box.offset, style, text_style, selection);
  // Pass the decoration_box to HighlightPainter so that the kOverlay path
  // respects text-decoration-skip-spaces trimming.
  if (skip_spaces != TextDecorationSkipSpaces::kNone &&
      style.HasAppliedTextDecorations()) {
    highlight_painter.SetOriginatingDecorationRect(decoration_box);
  }
  if (paint_info.phase == PaintPhase::kForeground) {
    if (auto* mf_checker = MobileFriendlinessChecker::From(document)) {
      if (auto* text = DynamicTo<LayoutText>(*layout_object)) {
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

  if (svg_inline_text) {
    TextPainter::SvgTextPaintState& svg_state = text_painter.SetSvgState(
        *svg_inline_text, style, text_item.GetStyleVariant(),
        paint_info.GetPaintFlags());

    if (scaling_factor != 1.0f) {
      state_saver.SaveIfNeeded();
      context.Scale(1 / scaling_factor, 1 / scaling_factor);
      svg_state.EnsureShaderTransform().Scale(scaling_factor);
    }
    if (text_item.HasSvgTransformForPaint()) {
      state_saver.SaveIfNeeded();
      const auto fragment_transform = text_item.BuildSvgTransformForPaint();
      context.ConcatCTM(fragment_transform);
      DCHECK(fragment_transform.IsInvertible());
      svg_state.EnsureShaderTransform().PostConcat(
          fragment_transform.Inverse());
    }
  }

  const bool paint_marker_backgrounds =
      paint_info.phase != PaintPhase::kSelectionDragImage &&
      paint_info.phase != PaintPhase::kTextClip && !is_printing;

  // 1. Paint backgrounds for document markers that don’t participate in the CSS
  // highlight overlay system, such as composition highlights. They use physical
  // coordinates, so are painted before GraphicsContext rotation.
  if (paint_marker_backgrounds) {
    highlight_painter.PaintNonCssMarkers(HighlightPainter::kBackground);
  }

  if (rotation) {
    state_saver.SaveIfNeeded();
    context.ConcatCTM(*rotation);
    if (TextPainter::SvgTextPaintState* state = text_painter.GetSvgState()) {
      DCHECK(rotation->IsInvertible());
      state->EnsureShaderTransform().PostConcat(rotation->Inverse());
    }
  }

  if (!interior_space_rects.empty()) {
    state_saver.SaveIfNeeded();
    for (const gfx::RectF& space_rect : interior_space_rects) {
      context.ClipOut(space_rect);
    }
  }

  if (highlight_painter.Selection()) [[unlikely]] {
    PhysicalRect physical_selection =
        highlight_painter.Selection()->PhysicalSelectionRect();
    if (svg_inline_text && scaling_factor != 1.0f) {
      physical_selection.offset.Scale(1 / scaling_factor);
      physical_selection.size.Scale(1 / scaling_factor);
    }

    // We need to use physical coordinates when invalidating.
    if (paint_marker_backgrounds && recorder) {
      recorder->UniteVisualRect(ToEnclosingRect(physical_selection));
    }
  }

  // 2. Now paint the foreground, including text and decorations.
  // TODO(dazabani@igalia.com): suppress text proper where one or more highlight
  // overlays are active, but paint shadows in full <https://crbug.com/1147859>
  if (ShouldPaintEmphasisMark(style, *layout_object, text_item)) {
    text_painter.SetEmphasisMark(style.TextEmphasisMarkString(),
                                 style.GetTextEmphasisLineLogicalSide(),
                                 &text_item);
  }

  DOMNodeId node_id = kInvalidDOMNodeId;
  if (node) {
    if (auto* layout_text = DynamicTo<LayoutText>(node->GetLayoutObject()))
      node_id = layout_text->EnsureNodeId();
  }
  InlinePaintContext::ScopedPaintOffset scoped_paint_offset(paint_offset,
                                                            inline_context_);

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kForeground));

  HighlightPainter::Case highlight_case = highlight_painter.PaintCase();
  switch (highlight_case) {
    case HighlightPainter::kNoHighlights:
    case HighlightPainter::kFastSpellingGrammar: {
      // Fast path: just paint the text, including its decorations.
      // Shadows must paint before decorations, but painting shadows in their
      // own pass is less efficient, so only do it when decorations are present.
      bool paint_shadows_first =
          text_style.shadow && style.HasAppliedTextDecorations();
      if (paint_shadows_first) {
        highlight_painter.PaintOriginatingShadow(text_style, node_id);
      }
      decoration_painter.Begin(text_item, TextDecorationPainter::kOriginating);
      decoration_painter.PaintExceptLineThrough(fragment_paint_info);
      {
        std::optional<GraphicsContextStateSaver> fit_text_state_saver;
        text_painter.ApplyTextFitScale(fragment_paint_info,
                                       &fit_text_state_saver);
        text_painter.Paint(
            fragment_paint_info, text_style, node_id, auto_dark_mode,
            paint_shadows_first ? TextPainter::kTextProperOnly
                                : TextPainter::kBothShadowsAndTextProper);
      }
      decoration_painter.PaintOnlyLineThrough();
      if (highlight_case == HighlightPainter::kFastSpellingGrammar) {
        highlight_painter.FastPaintSpellingGrammarDecorations();
      }
    } break;
    case HighlightPainter::kFastSelection:
      highlight_painter.Selection()->PaintSuppressingTextProperWhereSelected(
          text_painter, fragment_paint_info, text_style, node_id,
          auto_dark_mode);
      break;
    case HighlightPainter::kOverlay:
      // Paint originating shadows at the bottom, below all highlight pseudos.
      highlight_painter.PaintOriginatingShadow(text_style, node_id);
      // Paint each highlight overlay, including originating and selection.
      highlight_painter.PaintHighlightOverlays(
          text_style, node_id, paint_marker_backgrounds, rotation);
      break;
    case HighlightPainter::kSelectionOnly:
      // Do nothing, and paint the selection later.
      break;
  }

  // Paint ::selection background.
  if (highlight_painter.Selection() && paint_marker_backgrounds) [[unlikely]] {
    if (highlight_case == HighlightPainter::kFastSelection) {
      highlight_painter.Selection()->PaintSelectionBackground(
          context, node, document, style, paint_info, rotation);
    }
  }

  // Paint foregrounds for document markers that don’t participate in the CSS
  // highlight overlay system, such as composition highlights.
  if (paint_info.phase == PaintPhase::kForeground) {
    highlight_painter.PaintNonCssMarkers(HighlightPainter::kForeground);
  }

  // Paint ::selection foreground only.
  if (highlight_painter.Selection()) [[unlikely]] {
    switch (highlight_case) {
      case HighlightPainter::kFastSelection:
        highlight_painter.Selection()->PaintSelectedText(
            text_painter, fragment_paint_info, text_style, node_id,
            auto_dark_mode);
        break;
      case HighlightPainter::kSelectionOnly:
        decoration_painter.Begin(text_item, TextDecorationPainter::kSelection);
        decoration_painter.PaintExceptLineThrough(fragment_paint_info);
        highlight_painter.Selection()->PaintSelectedText(
            text_painter, fragment_paint_info, text_style, node_id,
            auto_dark_mode);
        decoration_painter.PaintOnlyLineThrough();
        break;
      case HighlightPainter::kOverlay:
        // Do nothing, because PaintHighlightOverlays already painted it.
        break;
      case HighlightPainter::kFastSpellingGrammar:
      case HighlightPainter::kNoHighlights:
        NOTREACHED();
    }
  }

  // Paint the character overlapping the block caret with the second value of
  // caret-color when it's non-auto.
  std::optional<unsigned> block_caret_char_offset;
  if (!is_printing && !is_rendering_resource &&
      paint_info.phase != PaintPhase::kTextClip &&
      RuntimeEnabledFeatures::CSSCaretColorWithOptionalSecondValueEnabled(
          layout_object->GetDocument().GetExecutionContext()) &&
      !layout_object->StyleRef().IsCaretTextColorAuto()) [[unlikely]] {
    if (LocalFrame* frame = layout_object->GetDocument().GetFrame()) {
      block_caret_char_offset =
          frame->Selection().ComputeBlockCaretCharacterOffset(cursor_);
    }
  }
  if (block_caret_char_offset) [[unlikely]] {
    std::optional<Color> block_text_color =
        layout_object->StyleRef().ResolvedCaretTextColor();
    if (block_text_color) {
      TextPaintStyle block_style = text_style;
      block_style.fill_color = *block_text_color;
      block_style.stroke_color = *block_text_color;
      block_style.emphasis_mark_color = *block_text_color;
      const unsigned char_start =
          fragment_paint_info.from + *block_caret_char_offset;
      const unsigned char_end = char_start + 1;
      // Clip to the character cell so that any cached SkTextBlob reused by
      // GraphicsContext::DrawText for the whole fragment can only repaint
      // this one character with the override color.
      PhysicalRect char_paint_rect =
          cursor_.CurrentLocalRect(char_start, char_end);
      char_paint_rect.Move(physical_box.offset);
      const LineRelativeRect char_clip_rect =
          LineRelativeRect::Create(char_paint_rect, rotation);
      GraphicsContextStateSaver clip_state_saver(context);
      context.Clip(gfx::RectF(char_clip_rect));
      std::optional<GraphicsContextStateSaver> fit_text_state_saver;
      text_painter.ApplyTextFitScale(fragment_paint_info,
                                     &fit_text_state_saver);
      text_painter.Paint(fragment_paint_info.WithStartOffset(char_start)
                             .WithEndOffset(char_end),
                         block_style, node_id, auto_dark_mode,
                         TextPainter::kTextProperOnly);
    }
  }
}

}  // namespace blink
