// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"

#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/document_marker_painter.h"
#include "third_party/blink/renderer/core/paint/line_relative_rect.h"
#include "third_party/blink/renderer/core/paint/ng/ng_highlight_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/selection_bounds_recorder.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

inline const DisplayItemClient& AsDisplayItemClient(const InlineCursor& cursor,
                                                    bool for_selection) {
  if (UNLIKELY(for_selection)) {
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
    absl::optional<InlineCursor>* storage) {
  if (*storage)
    return **storage;
  *storage = cursor;
  (*storage)->ExpandRootToContainingBlock();
  return **storage;
}

// Check if text-emphasis and ruby annotation text are on different sides.
// See InlineTextBox::GetEmphasisMarkPosition().
//
// TODO(layout-dev): The current behavior is compatible with the legacy layout.
// However, the specification asks to draw emphasis marks over ruby annotation
// text.
// https://drafts.csswg.org/css-text-decor-4/#text-emphasis-position-property
bool ShouldPaintEmphasisMark(const ComputedStyle& style,
                             const LayoutObject& layout_object) {
  if (style.GetTextEmphasisMark() == TextEmphasisMark::kNone)
    return false;
  // Note: We set text-emphasis-style:none for combined text and we paint
  // emphasis mark at left/right side of |LayoutNGTextCombine|.
  DCHECK(!IsA<LayoutTextCombine>(layout_object.Parent()));
  const LayoutObject* containing_block = layout_object.ContainingBlock();
  if (!containing_block || !containing_block->IsRubyBase())
    return true;
  const LayoutObject* parent = containing_block->Parent();
  if (!parent || !parent->IsRubyColumn()) {
    return true;
  }
  const auto* ruby_text = To<LayoutRubyColumn>(parent)->RubyText();
  if (!ruby_text)
    return true;
  if (!InlineCursor(*ruby_text)) {
    return true;
  }
  const LineLogicalSide ruby_logical_side =
      parent->StyleRef().GetRubyPosition() == RubyPosition::kBefore
          ? LineLogicalSide::kOver
          : LineLogicalSide::kUnder;
  return ruby_logical_side != style.GetTextEmphasisLineLogicalSide();
}

enum class DisclosureOrientation { kLeft, kRight, kUp, kDown };

DisclosureOrientation GetDisclosureOrientation(const ComputedStyle& style,
                                               bool is_open) {
  // TODO(layout-dev): Sideways-lr and sideways-rl are not yet supported.
  const auto mode = style.GetWritingMode();
  DCHECK_NE(mode, WritingMode::kSidewaysRl);
  DCHECK_NE(mode, WritingMode::kSidewaysLr);

  if (is_open) {
    if (blink::IsHorizontalWritingMode(mode)) {
      return DisclosureOrientation::kDown;
    }
    return IsFlippedBlocksWritingMode(mode) ? DisclosureOrientation::kLeft
                                            : DisclosureOrientation::kRight;
  }
  if (blink::IsHorizontalWritingMode(mode)) {
    return style.IsLeftToRightDirection() ? DisclosureOrientation::kRight
                                          : DisclosureOrientation::kLeft;
  }
  return style.IsLeftToRightDirection() ? DisclosureOrientation::kDown
                                        : DisclosureOrientation::kUp;
}

Path CreatePath(const gfx::PointF* path) {
  Path result;
  result.MoveTo(gfx::PointF(path[0].x(), path[0].y()));
  for (int i = 1; i < 4; ++i) {
    result.AddLineTo(gfx::PointF(path[i].x(), path[i].y()));
  }
  return result;
}

Path GetCanonicalDisclosurePath(const ComputedStyle& style, bool is_open) {
  constexpr gfx::PointF kLeftPoints[4] = {
      {1.0f, 0.0f}, {0.14f, 0.5f}, {1.0f, 1.0f}, {1.0f, 0.0f}};
  constexpr gfx::PointF kRightPoints[4] = {
      {0.0f, 0.0f}, {0.86f, 0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f}};
  constexpr gfx::PointF kUpPoints[4] = {
      {0.0f, 0.93f}, {0.5f, 0.07f}, {1.0f, 0.93f}, {0.0f, 0.93f}};
  constexpr gfx::PointF kDownPoints[4] = {
      {0.0f, 0.07f}, {0.5f, 0.93f}, {1.0f, 0.07f}, {0.0f, 0.07f}};

  switch (GetDisclosureOrientation(style, is_open)) {
    case DisclosureOrientation::kLeft:
      return CreatePath(kLeftPoints);
    case DisclosureOrientation::kRight:
      return CreatePath(kRightPoints);
    case DisclosureOrientation::kUp:
      return CreatePath(kUpPoints);
    case DisclosureOrientation::kDown:
      return CreatePath(kDownPoints);
  }

  return Path();
}

}  // namespace

void NGTextFragmentPainter::PaintSymbol(const LayoutObject* layout_object,
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
    color = TextPainterBase::TextColorForWhiteBackground(color);
  }
  // Apply the color to the list marker text.
  context.SetFillColor(color);
  context.SetStrokeColor(color);
  context.SetStrokeStyle(kSolidStroke);
  context.SetStrokeThickness(1.0f);
  const gfx::Rect snapped_rect = ToPixelSnappedRect(marker_rect);
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kListSymbol));
  if (type == keywords::kDisc) {
    context.FillEllipse(gfx::RectF(snapped_rect), auto_dark_mode);
  } else if (type == keywords::kCircle) {
    context.StrokeEllipse(gfx::RectF(snapped_rect), auto_dark_mode);
  } else if (type == keywords::kSquare) {
    context.FillRect(snapped_rect, color, auto_dark_mode);
  } else if (type == keywords::kDisclosureOpen ||
             type == keywords::kDisclosureClosed) {
    Path path =
        GetCanonicalDisclosurePath(style, type == keywords::kDisclosureOpen);
    path.Transform(AffineTransform::MakeScaleNonUniform(marker_rect.Width(),
                                                        marker_rect.Height()));
    path.Translate(gfx::Vector2dF(marker_rect.X(), marker_rect.Y()));
    context.FillPath(path, auto_dark_mode);
  } else {
    NOTREACHED();
  }
}

void NGTextFragmentPainter::Paint(const PaintInfo& paint_info,
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
  if (style.Visibility() != EVisibility::kVisible)
    return;

  const NGTextFragmentPaintInfo& fragment_paint_info =
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
  if (UNLIKELY(text_combine))
    LayoutTextCombine::AssertStyleIsValid(style);
#endif

  ObjectPainter object_painter(*layout_object);
  if (object_painter.ShouldRecordSpecialHitTestData(paint_info)) {
    object_painter.RecordHitTestData(paint_info,
                                     ToPixelSnappedRect(physical_box),
                                     *text_item.GetDisplayItemClient());
  }

  // Determine whether or not we’ll need a writing-mode rotation, but don’t
  // actually rotate until we reach the steps that need it.
  absl::optional<AffineTransform> rotation;
  const WritingMode writing_mode = style.GetWritingMode();
  const bool is_horizontal = IsHorizontalWritingMode(writing_mode);
  const LineRelativeRect rotated_box =
      LineRelativeRect::CreateFromLineBox(physical_box, is_horizontal);
  if (!is_horizontal) {
    rotation.emplace(
        rotated_box.ComputeRelativeToPhysicalTransform(writing_mode));
  }

  // Determine whether or not we're selected.
  NGHighlightPainter::SelectionPaintState* selection = nullptr;
  absl::optional<NGHighlightPainter::SelectionPaintState>
      selection_for_bounds_recording;
  if (UNLIKELY(!is_printing && !is_rendering_resource &&
               paint_info.phase != PaintPhase::kTextClip &&
               layout_object->IsSelected())) {
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
  if (UNLIKELY(svg_inline_text)) {
    DCHECK_EQ(text_item.Type(), FragmentItem::kSvgText);
    scaling_factor = svg_inline_text->ScalingFactor();
    DCHECK_NE(scaling_factor, 0.0f);
    visual_rect = gfx::ToEnclosingRect(
        svg_inline_text->Parent()->VisualRectInLocalSVGCoordinates());
  } else {
    DCHECK_NE(text_item.Type(), FragmentItem::kSvgText);
    PhysicalRect ink_overflow = text_item.SelfInkOverflow();
    ink_overflow.Move(physical_box.offset);
    visual_rect = ToEnclosingRect(ink_overflow);
  }

  // Ensure the selection bounds are recorded on the paint chunk regardless of
  // whether the display item that contains the actual selection painting is
  // reused.
  absl::optional<SelectionBoundsRecorder> selection_recorder;
  if (UNLIKELY(selection_for_bounds_recording &&
               paint_info.phase == PaintPhase::kForeground && !is_printing)) {
    if (SelectionBoundsRecorder::ShouldRecordSelection(
            cursor_.Current().GetLayoutObject()->GetFrame()->Selection(),
            selection_for_bounds_recording->State())) {
      selection_recorder.emplace(
          selection_for_bounds_recording->State(),
          selection_for_bounds_recording->PhysicalSelectionRect(),
          paint_info.context.GetPaintController(),
          cursor_.Current().ResolvedDirection(), style.GetWritingMode(),
          *cursor_.Current().GetLayoutObject());
    }
  }

  // This is declared after selection_recorder so that this will be destructed
  // before selection_recorder to ensure the selection is painted before
  // selection_recorder records the selection bounds.
  absl::optional<DrawingRecorder> recorder;
  const auto& display_item_client =
      AsDisplayItemClient(cursor_, selection != nullptr);
  // Text clips are initiated only in BoxPainterBase::PaintFillLayer, which is
  // already within a DrawingRecorder.
  if (paint_info.phase != PaintPhase::kTextClip) {
    if (LIKELY(!paint_info.context.InDrawingRecorder())) {
      if (DrawingRecorder::UseCachedDrawingIfPossible(
              paint_info.context, display_item_client, paint_info.phase)) {
        return;
      }
      recorder.emplace(paint_info.context, display_item_client,
                       paint_info.phase, visual_rect);
    }
  }

  if (UNLIKELY(text_item.IsSymbolMarker())) {
    PaintSymbol(layout_object, style, physical_box.size, paint_info,
                physical_box.offset);
    return;
  }

  GraphicsContext& context = paint_info.context;

  // Determine text colors.

  Node* node = layout_object->GetNode();
  TextPaintStyle text_style =
      TextPainterBase::TextPaintingStyle(document, style, paint_info);
  if (UNLIKELY(selection)) {
    selection->ComputeSelectionStyle(document, style, node, paint_info,
                                     text_style);
  }

  // Set our font.
  const Font& font =
      UNLIKELY(text_combine && text_combine->UsesCompressedFont())
          ? text_combine->CompressedFont()
          : text_item.ScaledFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);

  const bool paint_marker_backgrounds =
      paint_info.phase != PaintPhase::kSelectionDragImage &&
      paint_info.phase != PaintPhase::kTextClip && !is_printing;
  GraphicsContextStateSaver state_saver(context, /*save_and_restore=*/false);
  const int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  LineRelativeOffset text_origin{
      physical_box.offset.left,
      UNLIKELY(text_combine)
          ? text_combine->AdjustTextTopForPaint(physical_box.offset.top)
          : physical_box.offset.top + ascent};

  NGTextPainter text_painter(context, font, visual_rect, text_origin,
                             rotated_box, inline_context_, is_horizontal);
  NGTextDecorationPainter decoration_painter(text_painter, text_item,
                                             paint_info, style, text_style,
                                             rotated_box, selection);
  NGHighlightPainter highlight_painter(
      fragment_paint_info, text_painter, decoration_painter, paint_info,
      cursor_, *cursor_.CurrentItem(), rotation, physical_box.offset, style,
      text_style, selection, is_printing);
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
    NGTextPainter::SvgTextPaintState& svg_state = text_painter.SetSvgState(
        *svg_inline_text, style, text_item.StyleVariant(),
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

  // 1. Paint backgrounds for document markers that don’t participate in the CSS
  // highlight overlay system, such as composition highlights. They use physical
  // coordinates, so are painted before GraphicsContext rotation.
  highlight_painter.Paint(NGHighlightPainter::kBackground);

  if (rotation) {
    state_saver.SaveIfNeeded();
    context.ConcatCTM(*rotation);
    if (NGTextPainter::SvgTextPaintState* state = text_painter.GetSvgState()) {
      DCHECK(rotation->IsInvertible());
      state->EnsureShaderTransform().PostConcat(rotation->Inverse());
    }
  }

  if (UNLIKELY(highlight_painter.Selection())) {
    PhysicalRect physical_selection =
        highlight_painter.Selection()->PhysicalSelectionRect();
    if (scaling_factor != 1.0f) {
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
  if (ShouldPaintEmphasisMark(style, *layout_object)) {
    text_painter.SetEmphasisMark(style.TextEmphasisMarkString(),
                                 style.GetTextEmphasisPosition());
  }

  DOMNodeId node_id = kInvalidDOMNodeId;
  if (node) {
    if (auto* layout_text = DynamicTo<LayoutText>(node->GetLayoutObject()))
      node_id = layout_text->EnsureNodeId();
  }
  NGInlinePaintContext::ScopedPaintOffset scoped_paint_offset(paint_offset,
                                                              inline_context_);

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kForeground));

  NGHighlightPainter::Case highlight_case = highlight_painter.PaintCase();
  switch (highlight_case) {
    case NGHighlightPainter::kNoHighlights:
      // Fast path: just paint the text, including its decorations.
      decoration_painter.Begin(NGTextDecorationPainter::kOriginating);
      decoration_painter.PaintExceptLineThrough(fragment_paint_info);
      text_painter.Paint(fragment_paint_info, text_style, node_id,
                         auto_dark_mode);
      decoration_painter.PaintOnlyLineThrough();
      break;
    case NGHighlightPainter::kFastSpellingGrammar:
      decoration_painter.Begin(NGTextDecorationPainter::kOriginating);
      decoration_painter.PaintExceptLineThrough(fragment_paint_info);
      text_painter.Paint(fragment_paint_info, text_style, node_id,
                         auto_dark_mode);
      decoration_painter.PaintOnlyLineThrough();
      highlight_painter.FastPaintSpellingGrammarDecorations();
      break;
    case NGHighlightPainter::kFastSelection:
      highlight_painter.Selection()->PaintSuppressingTextProperWhereSelected(
          text_painter, fragment_paint_info, text_style, node_id,
          auto_dark_mode);
      break;
    case NGHighlightPainter::kOverlay:
      // Slow path: paint suppressing text proper where highlighted, then
      // paint each highlight overlay, suppressing unless topmost highlight.
      highlight_painter.PaintOriginatingText(text_style, node_id);
      highlight_painter.PaintHighlightOverlays(
          text_style, node_id, paint_marker_backgrounds, rotation);
      break;
    case NGHighlightPainter::kOldSelection:
      // Slow path: paint suppressing text proper where ::selection active.
      decoration_painter.Begin(NGTextDecorationPainter::kOriginating);
      decoration_painter.PaintExceptLineThrough(fragment_paint_info);
      highlight_painter.Selection()->PaintSuppressingTextProperWhereSelected(
          text_painter, fragment_paint_info, text_style, node_id,
          auto_dark_mode);
      decoration_painter.PaintOnlyLineThrough();
      break;
    case NGHighlightPainter::kSelectionOnly:
      // Do nothing, and paint the selection later.
      break;
  }

  // Paint ::selection background.
  if (UNLIKELY(highlight_painter.Selection() && paint_marker_backgrounds)) {
    if (highlight_case == NGHighlightPainter::kFastSelection ||
        highlight_case == NGHighlightPainter::kOldSelection) {
      highlight_painter.Selection()->PaintSelectionBackground(
          context, node, document, style, rotation);
    }
  }

  // Paint foregrounds for document markers that don’t participate in the CSS
  // highlight overlay system, such as composition highlights.
  if (paint_info.phase == PaintPhase::kForeground) {
    highlight_painter.Paint(NGHighlightPainter::kForeground);
  }

  // Paint ::selection foreground only.
  if (UNLIKELY(highlight_painter.Selection())) {
    switch (highlight_case) {
      case NGHighlightPainter::kFastSelection:
        highlight_painter.Selection()->PaintSelectedText(
            text_painter, fragment_paint_info, text_style, node_id,
            auto_dark_mode);
        break;
      case NGHighlightPainter::kSelectionOnly:
      case NGHighlightPainter::kOldSelection:
        decoration_painter.Begin(NGTextDecorationPainter::kSelection);
        decoration_painter.PaintExceptLineThrough(fragment_paint_info);
        highlight_painter.Selection()->PaintSelectedText(
            text_painter, fragment_paint_info, text_style, node_id,
            auto_dark_mode);
        decoration_painter.PaintOnlyLineThrough();
        break;
      case NGHighlightPainter::kOverlay:
        // Do nothing, because PaintHighlightOverlays already painted it.
        break;
      case NGHighlightPainter::kFastSpellingGrammar:
      case NGHighlightPainter::kNoHighlights:
        NOTREACHED();
    }
  }
}

}  // namespace blink
