// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"

#include "cc/input/layer_selection_bound.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/document_marker_painter.h"
#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"
#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"
#include "third_party/blink/renderer/core/paint/list_marker_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/selection_bounds_recorder.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

namespace {

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
  if (text_color == color)
    return Color(0xff - color.Red(), 0xff - color.Green(), 0xff - color.Blue());
  return color;
}

inline const DisplayItemClient& AsDisplayItemClient(
    const NGInlineCursor& cursor,
    bool for_selection) {
  if (UNLIKELY(for_selection)) {
    if (const auto* selection_client =
            cursor.Current().GetSelectionDisplayItemClient())
      return *selection_client;
  }
  return *cursor.Current().GetDisplayItemClient();
}

inline PhysicalRect ComputeBoxRect(const NGInlineCursor& cursor,
                                   const PhysicalOffset& paint_offset,
                                   const PhysicalOffset& parent_offset) {
  PhysicalRect box_rect = cursor.CurrentItem()->RectInContainerFragment();
  box_rect.offset.left += paint_offset.left;
  // We round the y-axis to ensure consistent line heights.
  box_rect.offset.top =
      LayoutUnit((paint_offset.top + parent_offset.top).Round()) +
      (box_rect.offset.top - parent_offset.top);
  return box_rect;
}

inline const NGInlineCursor& InlineCursorForBlockFlow(
    const NGInlineCursor& cursor,
    base::Optional<NGInlineCursor>* storage) {
  if (*storage)
    return **storage;
  *storage = cursor;
  (*storage)->ExpandRootToContainingBlock();
  return **storage;
}

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
  const base::Optional<unsigned>& ng_offset =
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

// Copied from InlineTextBoxPainter
void PaintDocumentMarkers(const PaintInfo& paint_info,
                          const NGFragmentItem& text_fragment,
                          StringView text,
                          const DocumentMarkerVector& markers_to_paint,
                          const PhysicalOffset& box_origin,
                          const ComputedStyle& style,
                          DocumentMarkerPaintPhase marker_paint_phase,
                          NGTextPainter* text_painter) {
  if (markers_to_paint.IsEmpty())
    return;

  DCHECK(text_fragment.GetNode());
  const auto& text_node = To<Text>(*text_fragment.GetNode());
  for (const DocumentMarker* marker : markers_to_paint) {
    const unsigned marker_start_offset =
        GetTextContentOffset(text_node, marker->StartOffset());
    const unsigned marker_end_offset =
        GetTextContentOffset(text_node, marker->EndOffset());
    const unsigned paint_start_offset =
        ClampOffset(marker_start_offset, text_fragment);
    const unsigned paint_end_offset =
        ClampOffset(marker_end_offset, text_fragment);
    if (paint_start_offset == paint_end_offset)
      continue;

    switch (marker->GetType()) {
      case DocumentMarker::kSpelling:
      case DocumentMarker::kGrammar: {
        if (text_fragment.GetNode()->GetDocument().Printing())
          break;
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground)
          continue;
        DocumentMarkerPainter::PaintDocumentMarker(
            paint_info, box_origin, style, marker->GetType(),
            MarkerRectForForeground(text_fragment, text, paint_start_offset,
                                    paint_end_offset));
      } break;

      case DocumentMarker::kTextFragment:
      case DocumentMarker::kTextMatch: {
        Node* node = text_fragment.GetNode();
        const Document& document = node->GetDocument();
        if (marker->GetType() == DocumentMarker::kTextMatch &&
            !document.GetFrame()->GetEditor().MarkedTextMatchesAreHighlighted())
          break;
        const auto& text_marker = To<TextMarkerBase>(*marker);
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground) {
          Color color;
          if (marker->GetType() == DocumentMarker::kTextMatch) {
            color = LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
                text_marker.IsActiveMatch(), document.InForcedColorsMode(),
                style.UsedColorScheme());
          } else {
            color = HighlightPaintingUtils::HighlightBackgroundColor(
                document, style, node, kPseudoIdTargetText);
          }
          PaintRect(paint_info.context, PhysicalOffset(box_origin),
                    text_fragment.LocalRect(text, paint_start_offset,
                                            paint_end_offset),
                    color);
          break;
        }

        const TextPaintStyle text_style =
            DocumentMarkerPainter::ComputeTextPaintStyleFrom(
                document, node, style, text_marker, paint_info);
        if (text_style.current_color == Color::kTransparent)
          break;
        text_painter->Paint(paint_start_offset, paint_end_offset,
                            paint_end_offset - paint_start_offset, text_style,
                            kInvalidDOMNodeId);
      } break;

      case DocumentMarker::kComposition:
      case DocumentMarker::kActiveSuggestion:
      case DocumentMarker::kSuggestion: {
        const auto& styleable_marker = To<StyleableMarker>(*marker);
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground) {
          PaintRect(paint_info.context, PhysicalOffset(box_origin),
                    text_fragment.LocalRect(text, paint_start_offset,
                                            paint_end_offset),
                    styleable_marker.BackgroundColor());
          break;
        }
        if (DocumentMarkerPainter::ShouldPaintMarkerUnderline(
                styleable_marker)) {
          const SimpleFontData* font_data = style.GetFont().PrimaryFont();
          DocumentMarkerPainter::PaintStyleableMarkerUnderline(
              paint_info.context, box_origin, styleable_marker, style,
              FloatRect(MarkerRectForForeground(
                  text_fragment, text, paint_start_offset, paint_end_offset)),
              LayoutUnit(font_data->GetFontMetrics().Height()),
              text_fragment.GetNode()->GetDocument().InDarkMode());
        }
      } break;

      default:
        NOTREACHED();
        break;
    }
  }
}

class SelectionPaintState {
  STACK_ALLOCATED();

 public:
  explicit SelectionPaintState(const NGInlineCursor& containing_block)
      : SelectionPaintState(containing_block,
                            containing_block.Current()
                                .GetLayoutObject()
                                ->GetDocument()
                                .GetFrame()
                                ->Selection()) {}
  explicit SelectionPaintState(const NGInlineCursor& containing_block,
                               const FrameSelection& frame_selection)
      : selection_status_(
            frame_selection.ComputeLayoutSelectionStatus(containing_block)),
        state_(frame_selection.ComputeLayoutSelectionStateForCursor(
            containing_block.Current())),
        containing_block_(containing_block) {}

  const LayoutSelectionStatus& Status() const { return selection_status_; }

  const TextPaintStyle& GetSelectionStyle() const { return selection_style_; }

  SelectionState State() const { return state_; }

  bool ShouldPaintSelectedTextOnly() const { return paint_selected_text_only_; }

  void ComputeSelectionStyle(const Document& document,
                             const ComputedStyle& style,
                             Node* node,
                             const PaintInfo& paint_info,
                             const TextPaintStyle& text_style) {
    selection_style_ = TextPainterBase::SelectionPaintingStyle(
        document, style, node, paint_info, text_style);
    paint_selected_text_only_ =
        (paint_info.phase == PaintPhase::kSelectionDragImage);
  }

  PhysicalRect ComputeSelectionRect(const PhysicalOffset& box_offset) {
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
  void PaintSelectionBackground(
      GraphicsContext& context,
      Node* node,
      const Document& document,
      const ComputedStyle& style,
      const base::Optional<AffineTransform>& rotation) {
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
  void MapSelectionRectIntoRotatedSpace(const AffineTransform& rotation) {
    DCHECK(selection_rect_);
    selection_rect_before_rotation_.emplace(*selection_rect_);
    *selection_rect_ = PhysicalRect::EnclosingRect(
        rotation.Inverse().MapRect(FloatRect(*selection_rect_)));
  }

  // Paint the selected text only.
  void PaintSelectedText(NGTextPainter& text_painter,
                         unsigned length,
                         const TextPaintStyle& text_style,
                         DOMNodeId node_id) {
    text_painter.PaintSelectedText(selection_status_.start,
                                   selection_status_.end, length, text_style,
                                   selection_style_, *selection_rect_, node_id);
  }

  // Paint the given text range in the given style, suppressing the text proper
  // (painting shadows only) where selected.
  void PaintSuppressingTextProperWhereSelected(NGTextPainter& text_painter,
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

 private:
  const LayoutSelectionStatus selection_status_;
  TextPaintStyle selection_style_;
  const SelectionState state_;
  base::Optional<PhysicalRect> selection_rect_;
  base::Optional<PhysicalRect> selection_rect_before_rotation_;
  const NGInlineCursor& containing_block_;
  bool paint_selected_text_only_;
};

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
  const LayoutObject* containing_block = layout_object.ContainingBlock();
  if (!containing_block || !containing_block->IsRubyBase())
    return true;
  const LayoutObject* parent = containing_block->Parent();
  if (!parent || !parent->IsRubyRun())
    return true;
  const LayoutRubyText* ruby_text = To<LayoutRubyRun>(parent)->RubyText();
  if (!ruby_text)
    return true;
  if (!NGInlineCursor(*ruby_text))
    return true;
  const LineLogicalSide ruby_logical_side =
      parent->StyleRef().GetRubyPosition() == RubyPosition::kBefore
          ? LineLogicalSide::kOver
          : LineLogicalSide::kUnder;
  return ruby_logical_side != style.GetTextEmphasisLineLogicalSide();
}

}  // namespace

void NGTextFragmentPainter::PaintSymbol(const LayoutObject* layout_object,
                                        const ComputedStyle& style,
                                        const PhysicalSize box_size,
                                        const PaintInfo& paint_info,
                                        const PhysicalOffset& paint_offset) {
  PhysicalRect marker_rect(
      ListMarker::RelativeSymbolMarkerRect(style, box_size.width));
  marker_rect.Move(paint_offset);
  ListMarkerPainter::PaintSymbol(paint_info, layout_object, style,
                                 marker_rect.ToLayoutRect());
}

// This is copied from InlineTextBoxPainter::PaintSelection() but lacks of
// ltr, expanding new line wrap or so which uses InlineTextBox functions.
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

  // Determine whether or not we're selected.
  base::Optional<SelectionPaintState> selection;
  if (UNLIKELY(!is_printing && paint_info.phase != PaintPhase::kTextClip &&
               layout_object->IsSelected())) {
    const NGInlineCursor& root_inline_cursor =
        InlineCursorForBlockFlow(cursor_, &inline_cursor_for_block_flow_);
    selection.emplace(root_inline_cursor);
    if (!selection->Status().HasValidRange())
      selection.reset();
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

  PhysicalRect box_rect = ComputeBoxRect(cursor_, paint_offset, parent_offset_);
  PhysicalRect ink_overflow = text_item.SelfInkOverflow();
  ink_overflow.Move(box_rect.offset);
  IntRect visual_rect = EnclosingIntRect(ink_overflow);

  // The text clip phase already has a DrawingRecorder. Text clips are initiated
  // only in BoxPainterBase::PaintFillLayer, which is already within a
  // DrawingRecorder.
  base::Optional<DrawingRecorder> recorder;
  const auto& display_item_client =
      AsDisplayItemClient(cursor_, selection.has_value());

  // Ensure the selection bounds are recorded on the paint chunk regardless of
  // whether the diplay item that contains the actual selection painting is
  // reused.
  base::Optional<SelectionBoundsRecorder> selection_recorder;
  if (UNLIKELY(selection && paint_info.phase == PaintPhase::kForeground &&
               !is_printing)) {
    if (SelectionBoundsRecorder::ShouldRecordSelection(
            cursor_.Current().GetLayoutObject()->GetFrame()->Selection(),
            selection->State())) {
      PhysicalRect selection_rect =
          selection->ComputeSelectionRect(box_rect.offset);
      selection_recorder.emplace(selection->State(), selection_rect,
                                 paint_info.context.GetPaintController());
    }
  }

  if (paint_info.phase != PaintPhase::kTextClip) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, display_item_client, paint_info.phase)) {
      return;
    }
    recorder.emplace(paint_info.context, display_item_client, paint_info.phase,
                     visual_rect);
  }

  if (UNLIKELY(text_item.IsSymbolMarker())) {
    // The NGInlineItem of marker might be Split(). To avoid calling PaintSymbol
    // multiple times, only call it the first time. For an outside marker, this
    // is when StartOffset is 0. But for an inside marker, the first StartOffset
    // can be greater due to leading bidi control characters like U+202A/U+202B,
    // U+202D/U+202E, U+2066/U+2067 or U+2068.
    DCHECK_LT(fragment_paint_info.from, fragment_paint_info.text.length());
    for (unsigned i = 0; i < fragment_paint_info.from; ++i) {
      if (!Character::IsBidiControl(fragment_paint_info.text.CodepointAt(i)))
        return;
    }
    PaintSymbol(layout_object, style, box_rect.size, paint_info,
                box_rect.offset);
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
  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);

  // 1. Paint backgrounds for document markers that don’t participate in the CSS
  // highlight overlay system, such as composition highlights. They use physical
  // coordinates, so are painted before GraphicsContext rotation.
  const DocumentMarkerVector& markers_to_paint =
      ComputeMarkersToPaint(node, text_item.IsEllipsis());
  const bool paint_marker_backgrounds =
      paint_info.phase != PaintPhase::kSelectionDragImage &&
      paint_info.phase != PaintPhase::kTextClip && !is_printing;
  if (paint_marker_backgrounds) {
    PaintDocumentMarkers(paint_info, text_item, cursor_.CurrentText(),
                         markers_to_paint, box_rect.offset, style,
                         DocumentMarkerPaintPhase::kBackground, nullptr);
  }

  base::Optional<GraphicsContextStateSaver> state_saver;
  base::Optional<AffineTransform> rotation;
  const WritingMode writing_mode = style.GetWritingMode();
  const bool is_horizontal = IsHorizontalWritingMode(writing_mode);
  if (!is_horizontal) {
    state_saver.emplace(context);
    // Because we rotate the GraphicsContext to match the logical direction,
    // transpose the |box_rect| to match to it.
    box_rect.size = PhysicalSize(box_rect.Height(), box_rect.Width());
    rotation.emplace(TextPainterBase::Rotation(
        box_rect, writing_mode != WritingMode::kSidewaysLr
                      ? TextPainterBase::kClockwise
                      : TextPainterBase::kCounterclockwise));
    context.ConcatCTM(*rotation);
  }

  if (UNLIKELY(selection)) {
    PhysicalRect before_rotation =
        selection->ComputeSelectionRect(box_rect.offset);

    // The selection rect is given in physical coordinates, so we need to map
    // them into our now-possibly-rotated space before calling any methods
    // that might rely on them. Best to do this immediately, because they are
    // cached internally and could potentially affect any method.
    if (rotation)
      selection->MapSelectionRectIntoRotatedSpace(*rotation);

    // We still need to use physical coordinates when invalidating.
    if (paint_marker_backgrounds && recorder)
      recorder->UniteVisualRect(EnclosingIntRect(before_rotation));
  }

  // 2. Now paint the foreground, including text and decorations.
  // TODO(dazabani@igalia.com): suppress text proper where one or more highlight
  // overlays are active, but paint shadows in full <https://crbug.com/1147859>
  int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  PhysicalOffset text_origin(box_rect.offset.left,
                             box_rect.offset.top + ascent);
  NGTextPainter text_painter(context, font, fragment_paint_info, visual_rect,
                             text_origin, box_rect, is_horizontal);

  if (ShouldPaintEmphasisMark(style, *layout_object)) {
    text_painter.SetEmphasisMark(style.TextEmphasisMarkString(),
                                 style.GetTextEmphasisPosition());
  }

  DOMNodeId node_id = kInvalidDOMNodeId;
  if (node) {
    if (auto* layout_text = DynamicTo<LayoutText>(node->GetLayoutObject()))
      node_id = layout_text->EnsureNodeId();
  }

  const unsigned length = fragment_paint_info.to - fragment_paint_info.from;
  if (!selection || !selection->ShouldPaintSelectedTextOnly()) {
    // Paint text decorations except line-through.
    base::Optional<TextDecorationInfo> decoration_info;
    bool has_line_through_decoration = false;
    if (style.TextDecorationsInEffect() != TextDecoration::kNone &&
        // Ellipsis should not have text decorations. This is not defined, but 4
        // impls do this.
        !text_item.IsEllipsis()) {
      PhysicalOffset local_origin = box_rect.offset;
      LayoutUnit width = box_rect.Width();
      base::Optional<AppliedTextDecoration> selection_text_decoration =
          UNLIKELY(selection)
              ? base::Optional<AppliedTextDecoration>(
                    selection->GetSelectionStyle().selection_text_decoration)
              : base::nullopt;

      decoration_info.emplace(box_rect.offset, local_origin, width,
                              style.GetFontBaseline(), style,
                              selection_text_decoration, nullptr);
      NGTextDecorationOffset decoration_offset(decoration_info->Style(),
                                               text_item.Style(), nullptr);
      text_painter.PaintDecorationsExceptLineThrough(
          decoration_offset, decoration_info.value(), paint_info,
          style.AppliedTextDecorations(), text_style,
          &has_line_through_decoration);
    }

    unsigned start_offset = fragment_paint_info.from;
    unsigned end_offset = fragment_paint_info.to;

    if (UNLIKELY(selection)) {
      selection->PaintSuppressingTextProperWhereSelected(
          text_painter, start_offset, end_offset, length, text_style, node_id);
    } else {
      text_painter.Paint(start_offset, end_offset, length, text_style, node_id);
    }

    // Paint line-through decoration if needed.
    if (has_line_through_decoration) {
      text_painter.PaintDecorationsOnlyLineThrough(
          decoration_info.value(), paint_info, style.AppliedTextDecorations(),
          text_style);
    }
  }

  // 3. Paint CSS highlight overlays, such as ::selection and ::target-text.
  // For each overlay, we paint its background, then its shadows, then the text
  // with any decorations it defines, and all of the ::selection overlay parts
  // are painted over any ::target-text overlay parts, and so on. The text
  // proper (as opposed to shadows) is only painted by the topmost overlay
  // applying to a piece of text (if any), and suppressed everywhere else.
  // TODO(dazabani@igalia.com): implement this for the other highlight pseudos
  if (UNLIKELY(selection)) {
    if (paint_marker_backgrounds) {
      selection->PaintSelectionBackground(context, node, document, style,
                                          rotation);
    }

    // Paint only the text that is selected.
    selection->PaintSelectedText(text_painter, length, text_style, node_id);
  }

  if (paint_info.phase != PaintPhase::kForeground)
    return;
  PaintDocumentMarkers(paint_info, text_item, cursor_.CurrentText(),
                       markers_to_paint, box_rect.offset, style,
                       DocumentMarkerPaintPhase::kForeground, &text_painter);
}

}  // namespace blink
