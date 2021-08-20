// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_highlight_painter.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/styleable_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_marker_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/paint/document_marker_painter.h"
#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"
#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

DocumentMarkerVector ComputeMarkersToPaint(Node* node, bool is_ellipsis) {
  // TODO(yoichio): Handle first-letter
  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return DocumentMarkerVector();
  // We don't paint any marker on ellipsis.
  if (is_ellipsis)
    return DocumentMarkerVector();

  DocumentMarkerController& document_marker_controller =
      node->GetDocument().Markers();
  return document_marker_controller.ComputeMarkersToPaint(*text_node);
}

unsigned GetTextContentOffset(const Text& text, unsigned offset) {
  // TODO(yoichio): Sanitize DocumentMarker around text length.
  const Position position(text, std::min(offset, text.length()));
  const NGOffsetMapping* const offset_mapping =
      NGOffsetMapping::GetFor(position);
  DCHECK(offset_mapping);
  const absl::optional<unsigned>& ng_offset =
      offset_mapping->GetTextContentOffset(position);
  DCHECK(ng_offset.has_value());
  return ng_offset.value();
}

// ClampOffset modifies |offset| fixed in a range of |text_fragment| start/end
// offsets.
// |offset| points not each character but each span between character.
// With that concept, we can clear catch what is inside start / end.
// Suppose we have "foo_bar"('_' is a space).
// There are 8 offsets for that:
//  f o o _ b a r
// 0 1 2 3 4 5 6 7
// If "bar" is a TextFragment. That start(), end() {4, 7} correspond this
// offset. If a marker has StartOffset / EndOffset as {2, 6},
// ClampOffset returns{ 4,6 }, which represents "ba" on "foo_bar".
unsigned ClampOffset(unsigned offset, const NGFragmentItem& text_fragment) {
  return std::min(std::max(offset, text_fragment.StartOffset()),
                  text_fragment.EndOffset());
}

PhysicalRect MarkerRectForForeground(const NGFragmentItem& text_fragment,
                                     StringView text,
                                     unsigned start_offset,
                                     unsigned end_offset) {
  LayoutUnit start_position, end_position;
  std::tie(start_position, end_position) =
      text_fragment.LineLeftAndRightForOffsets(text, start_offset, end_offset);

  const LayoutUnit height = text_fragment.Size()
                                .ConvertToLogical(static_cast<WritingMode>(
                                    text_fragment.Style().GetWritingMode()))
                                .block_size;
  return {start_position, LayoutUnit(), end_position - start_position, height};
}

void PaintRect(GraphicsContext& context,
               const PhysicalRect& rect,
               const Color color) {
  if (!color.Alpha())
    return;
  if (rect.size.IsEmpty())
    return;
  const IntRect pixel_snapped_rect = PixelSnappedIntRect(rect);
  if (!pixel_snapped_rect.IsEmpty())
    context.FillRect(pixel_snapped_rect, color);
}

void PaintRect(GraphicsContext& context,
               const PhysicalOffset& location,
               const PhysicalRect& rect,
               const Color color) {
  PaintRect(context, PhysicalRect(rect.offset + location, rect.size), color);
}

Color SelectionBackgroundColor(const Document& document,
                               const ComputedStyle& style,
                               Node* node,
                               Color text_color) {
  const Color color = HighlightPaintingUtils::HighlightBackgroundColor(
      document, style, node, kPseudoIdSelection);
  if (!color.Alpha())
    return Color();

  // If the text color ends up being the same as the selection background,
  // invert the selection background.
  if (text_color == color) {
    UseCounter::Count(node->GetDocument(),
                      WebFeature::kSelectionBackgroundColorInversion);
    return Color(0xff - color.Red(), 0xff - color.Green(), 0xff - color.Blue());
  }
  return color;
}

}  // namespace

NGHighlightPainter::SelectionPaintState::SelectionPaintState(
    const NGInlineCursor& containing_block)
    : SelectionPaintState(containing_block,
                          containing_block.Current()
                              .GetLayoutObject()
                              ->GetDocument()
                              .GetFrame()
                              ->Selection()) {}
NGHighlightPainter::SelectionPaintState::SelectionPaintState(
    const NGInlineCursor& containing_block,
    const FrameSelection& frame_selection)
    : selection_status_(
          frame_selection.ComputeLayoutSelectionStatus(containing_block)),
      state_(frame_selection.ComputeLayoutSelectionStateForCursor(
          containing_block.Current())),
      containing_block_(containing_block) {}

void NGHighlightPainter::SelectionPaintState::ComputeSelectionStyle(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    const PaintInfo& paint_info,
    const TextPaintStyle& text_style) {
  selection_style_ = TextPainterBase::SelectionPaintingStyle(
      document, style, node, paint_info, text_style);
  paint_selected_text_only_ =
      (paint_info.phase == PaintPhase::kSelectionDragImage);
}

PhysicalRect NGHighlightPainter::SelectionPaintState::ComputeSelectionRect(
    const PhysicalOffset& box_offset) {
  if (!selection_rect_) {
    selection_rect_ =
        containing_block_.CurrentLocalSelectionRectForText(selection_status_);
    selection_rect_->offset += box_offset;
  }
  return *selection_rect_;
}

// Logic is copied from InlineTextBoxPainter::PaintSelection.
// |selection_start| and |selection_end| should be between
// [text_fragment.StartOffset(), text_fragment.EndOffset()].
void NGHighlightPainter::SelectionPaintState::PaintSelectionBackground(
    GraphicsContext& context,
    Node* node,
    const Document& document,
    const ComputedStyle& style,
    const absl::optional<AffineTransform>& rotation) {
  const Color color = SelectionBackgroundColor(document, style, node,
                                               selection_style_.fill_color);

  if (!rotation) {
    PaintRect(context, *selection_rect_, color);
    return;
  }

  // PaintRect tries to pixel-snap the given rect, but if we’re painting in a
  // non-horizontal writing mode, our context has been transformed, regressing
  // tests like <paint/invalidation/repaint-across-writing-mode-boundary>. To
  // fix this, we undo the transformation temporarily, then use the original
  // physical coordinates (before MapSelectionRectIntoRotatedSpace).
  DCHECK(selection_rect_before_rotation_);
  context.ConcatCTM(rotation->Inverse());
  PaintRect(context, *selection_rect_before_rotation_, color);
  context.ConcatCTM(*rotation);
}

// Called before we paint vertical selected text under a rotation transform.
void NGHighlightPainter::SelectionPaintState::MapSelectionRectIntoRotatedSpace(
    const AffineTransform& rotation) {
  DCHECK(selection_rect_);
  selection_rect_before_rotation_.emplace(*selection_rect_);
  *selection_rect_ = PhysicalRect::EnclosingRect(
      rotation.Inverse().MapRect(FloatRect(*selection_rect_)));
}

// Paint the selected text only.
void NGHighlightPainter::SelectionPaintState::PaintSelectedText(
    NGTextPainter& text_painter,
    unsigned length,
    const TextPaintStyle& text_style,
    DOMNodeId node_id) {
  text_painter.PaintSelectedText(selection_status_.start, selection_status_.end,
                                 length, text_style, selection_style_,
                                 *selection_rect_, node_id);
}

// Paint the given text range in the given style, suppressing the text proper
// (painting shadows only) where selected.
void NGHighlightPainter::SelectionPaintState::
    PaintSuppressingTextProperWhereSelected(NGTextPainter& text_painter,
                                            unsigned start_offset,
                                            unsigned end_offset,
                                            unsigned length,
                                            const TextPaintStyle& text_style,
                                            DOMNodeId node_id) {
  // First paint the shadows for the whole range.
  if (text_style.shadow) {
    text_painter.Paint(start_offset, end_offset, length, text_style, node_id,
                       NGTextPainter::kShadowsOnly);
  }

  // Then paint the text proper for any unselected parts in storage order, so
  // that they’re always on top of the shadows.
  if (start_offset < selection_status_.start) {
    text_painter.Paint(start_offset, selection_status_.start, length,
                       text_style, node_id, NGTextPainter::kTextProperOnly);
  }
  if (selection_status_.end < end_offset) {
    text_painter.Paint(selection_status_.end, end_offset, length, text_style,
                       node_id, NGTextPainter::kTextProperOnly);
  }
}

NGHighlightPainter::NGHighlightPainter(NGTextPainter& text_painter,
                                       const PaintInfo& paint_info,
                                       const NGInlineCursor& cursor,
                                       const NGFragmentItem& fragment_item,
                                       const PhysicalOffset& box_origin,
                                       const ComputedStyle& style,
                                       SelectionPaintState* selection,
                                       bool is_printing)
    : text_painter_(text_painter),
      paint_info_(paint_info),
      cursor_(cursor),
      fragment_item_(fragment_item),
      box_origin_(box_origin),
      style_(style),
      selection_(selection),
      layout_object_(fragment_item_.GetLayoutObject()),
      node_(layout_object_->GetNode()),
      markers_(ComputeMarkersToPaint(node_, fragment_item_.IsEllipsis())),
      skip_backgrounds_(is_printing ||
                        paint_info.phase == PaintPhase::kTextClip ||
                        paint_info.phase == PaintPhase::kSelectionDragImage) {}

void NGHighlightPainter::Paint(Phase phase) {
  if (markers_.IsEmpty())
    return;

  if (skip_backgrounds_ && phase == kBackground)
    return;

  DCHECK(fragment_item_.GetNode());
  const auto& text_node = To<Text>(*fragment_item_.GetNode());
  const StringView text = cursor_.CurrentText();

  for (const DocumentMarker* marker : markers_) {
    const unsigned marker_start_offset =
        GetTextContentOffset(text_node, marker->StartOffset());
    const unsigned marker_end_offset =
        GetTextContentOffset(text_node, marker->EndOffset());
    const unsigned paint_start_offset =
        ClampOffset(marker_start_offset, fragment_item_);
    const unsigned paint_end_offset =
        ClampOffset(marker_end_offset, fragment_item_);
    if (paint_start_offset == paint_end_offset)
      continue;

    switch (marker->GetType()) {
      case DocumentMarker::kSpelling:
      case DocumentMarker::kGrammar: {
        if (fragment_item_.GetNode()->GetDocument().Printing())
          break;
        if (phase == kBackground)
          continue;
        DocumentMarkerPainter::PaintDocumentMarker(
            paint_info_, box_origin_, style_, marker->GetType(),
            MarkerRectForForeground(fragment_item_, text, paint_start_offset,
                                    paint_end_offset));
      } break;

      case DocumentMarker::kTextFragment:
      case DocumentMarker::kTextMatch: {
        const Document& document = node_->GetDocument();
        if (marker->GetType() == DocumentMarker::kTextMatch &&
            !document.GetFrame()->GetEditor().MarkedTextMatchesAreHighlighted())
          break;
        const auto& text_marker = To<TextMarkerBase>(*marker);
        if (phase == kBackground) {
          Color color;
          if (marker->GetType() == DocumentMarker::kTextMatch) {
            color = LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
                text_marker.IsActiveMatch(), style_.UsedColorScheme());
          } else {
            color = HighlightPaintingUtils::HighlightBackgroundColor(
                document, style_, node_, kPseudoIdTargetText);
          }
          PaintRect(paint_info_.context, PhysicalOffset(box_origin_),
                    fragment_item_.LocalRect(text, paint_start_offset,
                                             paint_end_offset),
                    color);
          break;
        }

        const TextPaintStyle text_style =
            DocumentMarkerPainter::ComputeTextPaintStyleFrom(
                document, node_, style_, text_marker, paint_info_);
        if (text_style.current_color == Color::kTransparent)
          break;
        text_painter_.Paint(paint_start_offset, paint_end_offset,
                            paint_end_offset - paint_start_offset, text_style,
                            kInvalidDOMNodeId);
      } break;

      case DocumentMarker::kComposition:
      case DocumentMarker::kActiveSuggestion:
      case DocumentMarker::kSuggestion: {
        const auto& styleable_marker = To<StyleableMarker>(*marker);
        if (phase == kBackground) {
          PaintRect(paint_info_.context, PhysicalOffset(box_origin_),
                    fragment_item_.LocalRect(text, paint_start_offset,
                                             paint_end_offset),
                    styleable_marker.BackgroundColor());
          break;
        }
        if (DocumentMarkerPainter::ShouldPaintMarkerUnderline(
                styleable_marker)) {
          const SimpleFontData* font_data = style_.GetFont().PrimaryFont();
          DocumentMarkerPainter::PaintStyleableMarkerUnderline(
              paint_info_.context, box_origin_, styleable_marker, style_,
              FloatRect(MarkerRectForForeground(
                  fragment_item_, text, paint_start_offset, paint_end_offset)),
              LayoutUnit(font_data->GetFontMetrics().Height()),
              fragment_item_.GetNode()->GetDocument().InDarkMode());
        }
      } break;

      case DocumentMarker::kHighlight: {
        const auto& highlight_marker = To<HighlightMarker>(*marker);
        const Document& document = node_->GetDocument();

        // Paint background
        if (phase == kBackground) {
          Color background_color =
              HighlightPaintingUtils::HighlightBackgroundColor(
                  document, style_, node_, kPseudoIdHighlight,
                  highlight_marker.GetHighlightName());

          PaintRect(paint_info_.context, PhysicalOffset(box_origin_),
                    fragment_item_.LocalRect(text, paint_start_offset,
                                             paint_end_offset),
                    background_color);
          break;
        }

        DCHECK_EQ(phase, kForeground);
        Color text_color = style_.VisitedDependentColor(GetCSSPropertyColor());

        TextPaintStyle text_style;
        text_style.current_color = text_style.fill_color =
            text_style.stroke_color = text_style.emphasis_mark_color =
                text_color;
        text_style.stroke_width = style_.TextStrokeWidth();
        text_style.color_scheme = style_.UsedColorScheme();
        text_style.shadow = nullptr;

        const TextPaintStyle final_text_style =
            HighlightPaintingUtils::HighlightPaintingStyle(
                document, style_, node_, kPseudoIdHighlight, text_style,
                paint_info_, highlight_marker.GetHighlightName());

        if (final_text_style.current_color == Color::kTransparent)
          break;

        text_painter_.Paint(paint_start_offset, paint_end_offset,
                            paint_end_offset - paint_start_offset,
                            final_text_style, kInvalidDOMNodeId);

      } break;

      default:
        NOTREACHED();
        break;
    }
  }
}

}  // namespace blink
