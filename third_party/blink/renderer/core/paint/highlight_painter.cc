// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/highlight_painter.h"

#include "base/not_fatal_until.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/styleable_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/text_offset_range.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/highlight_overlay.h"
#include "third_party/blink/renderer/core/paint/line_relative_rect.h"
#include "third_party/blink/renderer/core/paint/marker_range_mapping_context.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/styleable_marker_painter.h"
#include "third_party/blink/renderer/core/paint/text_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/text_painter.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

using HighlightLayerType = HighlightOverlay::HighlightLayerType;
using HighlightRange = HighlightOverlay::HighlightRange;
using HighlightEdge = HighlightOverlay::HighlightEdge;
using HighlightDecoration = HighlightOverlay::HighlightDecoration;
using HighlightBackground = HighlightOverlay::HighlightBackground;
using HighlightTextShadow = HighlightOverlay::HighlightTextShadow;

LineRelativeRect LineRelativeLocalRect(const FragmentItem& text_fragment,
                                       StringView text,
                                       unsigned start_offset,
                                       unsigned end_offset) {
  LayoutUnit start_position, end_position;
  std::tie(start_position, end_position) =
      text_fragment.LineLeftAndRightForOffsets(text, start_offset, end_offset);

  const LayoutUnit height = text_fragment.InkOverflowRect().Height();
  return {{start_position, LayoutUnit()},
          {end_position - start_position, height}};
}

void PaintRect(GraphicsContext& context,
               const PhysicalRect& rect,
               const Color color,
               const AutoDarkMode& auto_dark_mode) {
  if (color.IsFullyTransparent()) {
    return;
  }
  if (rect.size.IsEmpty())
    return;
  const gfx::Rect pixel_snapped_rect = ToPixelSnappedRect(rect);
  if (!pixel_snapped_rect.IsEmpty())
    context.FillRect(pixel_snapped_rect, color, auto_dark_mode);
}

const LayoutSelectionStatus* GetSelectionStatus(
    const HighlightPainter::SelectionPaintState* selection) {
  if (!selection)
    return nullptr;
  return &selection->Status();
}

// Returns true if the styles for the given spelling or grammar pseudo require
// the full overlay painting algorithm.
bool HasNonTrivialSpellingGrammarStyles(const FragmentItem& fragment_item,
                                        Node* node,
                                        const ComputedStyle& originating_style,
                                        PseudoId pseudo) {
  DCHECK(pseudo == kPseudoIdSpellingError || pseudo == kPseudoIdGrammarError);
  if (const ComputedStyle* pseudo_style =
          HighlightStyleUtils::HighlightPseudoStyle(node, originating_style,
                                                    pseudo)) {
    const Document& document = node->GetDocument();
    // If the ‘color’, ‘-webkit-text-fill-color’, ‘-webkit-text-stroke-color’,
    // or ‘-webkit-text-stroke-width’ differs from the originating style.
    Color pseudo_color = HighlightStyleUtils::ResolveColor(
        document, originating_style, pseudo_style, pseudo,
        GetCSSPropertyColor(), {}, SearchTextIsCurrent::kNo);
    if (pseudo_color !=
        originating_style.VisitedDependentColor(GetCSSPropertyColor())) {
      return true;
    }
    if (HighlightStyleUtils::ResolveColor(document, originating_style,
                                          pseudo_style, pseudo,
                                          GetCSSPropertyWebkitTextFillColor(),
                                          {}, SearchTextIsCurrent::kNo) !=
        originating_style.VisitedDependentColor(
            GetCSSPropertyWebkitTextFillColor())) {
      return true;
    }
    if (HighlightStyleUtils::ResolveColor(document, originating_style,
                                          pseudo_style, pseudo,
                                          GetCSSPropertyWebkitTextStrokeColor(),
                                          {}, SearchTextIsCurrent::kNo) !=
        originating_style.VisitedDependentColor(
            GetCSSPropertyWebkitTextStrokeColor())) {
      return true;
    }
    if (pseudo_style->TextStrokeWidth() != originating_style.TextStrokeWidth())
      return true;
    // If there is a background color.
    if (!HighlightStyleUtils::ResolveColor(
             document, originating_style, pseudo_style, pseudo,
             GetCSSPropertyBackgroundColor(), {}, SearchTextIsCurrent::kNo)
             .IsFullyTransparent()) {
      return true;
    }
    // If the ‘text-shadow’ is not ‘none’.
    if (pseudo_style->TextShadow())
      return true;

    // If the ‘text-decoration-line’ is not ‘spelling-error’ or ‘grammar-error’,
    // depending on the pseudo. ‘text-decoration-color’ can vary without hurting
    // the optimisation, and for these line types, we ignore all other text
    // decoration related properties anyway.
    if (pseudo_style->TextDecorationsInEffect() !=
        (pseudo == kPseudoIdSpellingError
             ? TextDecorationLine::kSpellingError
             : TextDecorationLine::kGrammarError)) {
      return true;
    }
    // If any of the originating line decorations would need to be recolored.
    for (const AppliedTextDecoration& decoration :
         originating_style.AppliedTextDecorations()) {
      if (decoration.GetColor() != pseudo_color) {
        return true;
      }
    }
    // ‘text-emphasis-color’ should be meaningless for highlight pseudos, but
    // in our current impl, it sets the color of originating emphasis marks.
    // This means we can only use kFastSpellingGrammar if the color is the same
    // as in the originating style, or there are no emphasis marks.
    // TODO(crbug.com/1147859) clean up when spec issue is resolved again
    // https://github.com/w3c/csswg-drafts/issues/7101
    if (originating_style.GetTextEmphasisMark() != TextEmphasisMark::kNone &&
        HighlightStyleUtils::ResolveColor(
            document, originating_style, pseudo_style, pseudo,
            GetCSSPropertyTextEmphasisColor(), {}, SearchTextIsCurrent::kNo) !=
            originating_style.VisitedDependentColor(
                GetCSSPropertyTextEmphasisColor())) {
      return true;
    }
    // If the SVG-only fill- and stroke-related properties differ from their
    // values in the originating style. These checks must be skipped outside of
    // SVG content, because the initial ‘fill’ is ‘black’, not ‘currentColor’.
    if (fragment_item.IsSvgText()) {
      // If the ‘fill’ is ‘currentColor’, assume that it differs from the
      // originating style, even if the current color actually happens to
      // match. This simplifies the logic until we know it performs poorly.
      if (pseudo_style->FillPaint().HasCurrentColor())
        return true;
      // If the ‘fill’ differs from the originating style.
      if (pseudo_style->FillPaint() != originating_style.FillPaint())
        return true;
      // If the ‘stroke’ is ‘currentColor’, assume that it differs from the
      // originating style, even if the current color actually happens to
      // match. This simplifies the logic until we know it performs poorly.
      if (pseudo_style->StrokePaint().HasCurrentColor())
        return true;
      // If the ‘stroke’ differs from the originating style.
      if (pseudo_style->StrokePaint() != originating_style.StrokePaint())
        return true;
      // If the ‘stroke-width’ differs from the originating style.
      if (pseudo_style->StrokeWidth() != originating_style.StrokeWidth())
        return true;
    }
  }
  return false;
}

TextPaintStyle TextPaintStyleForTextMatch(const TextMatchMarker& marker,
                                          const ComputedStyle& style,
                                          const Document& document,
                                          bool ignore_current_color) {
  const mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  const Color platform_text_color =
      LayoutTheme::GetTheme().PlatformTextSearchColor(
          marker.IsActiveMatch(), document.InForcedColorsMode(), color_scheme,
          document.GetColorProviderForPainting(color_scheme),
          document.IsInWebAppScope());
  // Comparing against the value of the 'color' property doesn't always make
  // sense (for example for SVG <text> which paints using 'fill' and 'stroke').
  if (!ignore_current_color) {
    const Color text_color = style.VisitedDependentColor(GetCSSPropertyColor());
    if (platform_text_color == text_color) {
      return {};
    }
  }

  TextPaintStyle text_style;
  text_style.current_color = text_style.fill_color = text_style.stroke_color =
      text_style.emphasis_mark_color = platform_text_color;
  text_style.stroke_width = style.TextStrokeWidth();
  text_style.color_scheme = color_scheme;
  text_style.shadow = nullptr;
  text_style.paint_order = style.PaintOrder();
  return text_style;
}

// A contiguous run of parts that can have ‘background-color’ or ‘text-shadow’
// of some active overlay painted at once.
//
// These properties can often be painted a whole highlighted range at a time,
// and only need to be split into parts when affected by ‘currentColor’. By
// merging parts where possible, we avoid creating unnecessary “seams” in
// ‘background-color’, and avoid splitting ligatures in ‘text-shadow’.
//
// Inner’s operator== must return true iff the two operands come from the same
// layer and can be painted at once.
template <typename Inner>
struct MergedHighlightPart {
 public:
  struct Merged {
   public:
    const Inner& inner;
    unsigned from;
    unsigned to;
  };

  // Merge |next| and |next_part| into |merged| if possible, otherwise start a
  // new run and return the old |merged| if any.
  std::optional<Merged> Merge(const Inner& next,
                              const HighlightPart& next_part) {
    std::optional<Merged> result{};
    if (merged.has_value()) {
      if (merged->inner == next && merged->to == next_part.range.from) {
        merged->to = next_part.range.to;
        return {};
      } else {
        result.emplace(*merged);
      }
    }
    merged.emplace(Merged{next, next_part.range.from, next_part.range.to});
    return result;
  }

  // Take and return the last |merged| if any, leaving it empty.
  std::optional<Merged> Take() {
    if (!merged.has_value()) {
      return {};
    }
    std::optional<Merged> result{};
    result.emplace(*merged);
    merged.reset();
    return result;
  }

  std::optional<Merged> merged;
};

}  // namespace

HighlightPainter::SelectionPaintState::SelectionPaintState(
    const InlineCursor& containing_block,
    const PhysicalOffset& box_offset,
    const std::optional<AffineTransform> writing_mode_rotation)
    : SelectionPaintState(containing_block,
                          box_offset,
                          writing_mode_rotation,
                          containing_block.Current()
                              .GetLayoutObject()
                              ->GetDocument()
                              .GetFrame()
                              ->Selection()) {}
HighlightPainter::SelectionPaintState::SelectionPaintState(
    const InlineCursor& containing_block,
    const PhysicalOffset& box_offset,
    const std::optional<AffineTransform> writing_mode_rotation,
    const FrameSelection& frame_selection)
    : selection_status_(
          frame_selection.ComputeLayoutSelectionStatus(containing_block)),
      state_(frame_selection.ComputePaintingSelectionStateForCursor(
          containing_block.Current())),
      containing_block_(containing_block),
      box_offset_(box_offset),
      writing_mode_rotation_(writing_mode_rotation) {}

void HighlightPainter::SelectionPaintState::ComputeSelectionStyle(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    const PaintInfo& paint_info,
    const TextPaintStyle& text_style) {
  const ComputedStyle* pseudo_style = HighlightStyleUtils::HighlightPseudoStyle(
      node, style, kPseudoIdSelection);
  selection_style_ = HighlightStyleUtils::HighlightPaintingStyle(
      document, style, pseudo_style, node, kPseudoIdSelection, text_style,
      paint_info, SearchTextIsCurrent::kNo);
  paint_selected_text_only_ =
      (paint_info.phase == PaintPhase::kSelectionDragImage);
}

void HighlightPainter::SelectionPaintState::ComputeSelectionRectIfNeeded() {
  if (!selection_rect_) {
    PhysicalRect physical =
        containing_block_.CurrentLocalSelectionRectForText(selection_status_);
    physical.offset += box_offset_;
    LineRelativeRect rotated =
        LineRelativeRect::Create(physical, writing_mode_rotation_);
    selection_rect_.emplace(SelectionRect{physical, rotated});
  }
}

const PhysicalRect&
HighlightPainter::SelectionPaintState::PhysicalSelectionRect() {
  ComputeSelectionRectIfNeeded();
  return selection_rect_->physical;
}

const LineRelativeRect&
HighlightPainter::SelectionPaintState::LineRelativeSelectionRect() {
  ComputeSelectionRectIfNeeded();
  return selection_rect_->rotated;
}

// |selection_start| and |selection_end| should be between
// [text_fragment.StartOffset(), text_fragment.EndOffset()].
void HighlightPainter::SelectionPaintState::PaintSelectionBackground(
    GraphicsContext& context,
    Node* node,
    const Document& document,
    const ComputedStyle& style,
    const std::optional<AffineTransform>& rotation) {
  const Color color = HighlightStyleUtils::HighlightBackgroundColor(
      document, style, node, selection_style_.style.current_color,
      kPseudoIdSelection, SearchTextIsCurrent::kNo);
  HighlightPainter::PaintHighlightBackground(context, style, color,
                                             PhysicalSelectionRect(), rotation);
}

// Paint the selected text only.
void HighlightPainter::SelectionPaintState::PaintSelectedText(
    TextPainter& text_painter,
    const TextFragmentPaintInfo& fragment_paint_info,
    const TextPaintStyle& text_style,
    DOMNodeId node_id,
    const AutoDarkMode& auto_dark_mode) {
  text_painter.PaintSelectedText(fragment_paint_info, selection_status_.start,
                                 selection_status_.end, text_style,
                                 selection_style_.style, LineRelativeSelectionRect(),
                                 node_id, auto_dark_mode);
}

// Paint the given text range in the given style, suppressing the text proper
// (painting shadows only) where selected.
void HighlightPainter::SelectionPaintState::
    PaintSuppressingTextProperWhereSelected(
        TextPainter& text_painter,
        const TextFragmentPaintInfo& fragment_paint_info,
        const TextPaintStyle& text_style,
        DOMNodeId node_id,
        const AutoDarkMode& auto_dark_mode) {
  // First paint the shadows for the whole range.
  if (text_style.shadow) {
    text_painter.Paint(fragment_paint_info, text_style, node_id, auto_dark_mode,
                       TextPainter::kShadowsOnly);
  }

  // Then paint the text proper for any unselected parts in storage order, so
  // that they’re always on top of the shadows.
  if (fragment_paint_info.from < selection_status_.start) {
    text_painter.Paint(
        fragment_paint_info.WithEndOffset(selection_status_.start), text_style,
        node_id, auto_dark_mode, TextPainter::kTextProperOnly);
  }
  if (selection_status_.end < fragment_paint_info.to) {
    text_painter.Paint(
        fragment_paint_info.WithStartOffset(selection_status_.end), text_style,
        node_id, auto_dark_mode, TextPainter::kTextProperOnly);
  }
}

// GetNode() for first-letter fragment returns null because it is anonymous.
// Use AssociatedTextNode() of LayoutTextFragment to get the associated node.
static Node* AssociatedNode(const LayoutObject* layout_object) {
  if (RuntimeEnabledFeatures::PaintHighlightsForFirstLetterEnabled()) {
    if (auto* layout_text_fragment =
            DynamicTo<LayoutTextFragment>(layout_object)) {
      return layout_text_fragment->AssociatedTextNode();
    }
  }
  return layout_object->GetNode();
}

HighlightPainter::HighlightPainter(
    const TextFragmentPaintInfo& fragment_paint_info,
    TextPainter& text_painter,
    TextDecorationPainter& decoration_painter,
    const PaintInfo& paint_info,
    const InlineCursor& cursor,
    const FragmentItem& fragment_item,
    const PhysicalOffset& box_origin,
    const ComputedStyle& style,
    const TextPaintStyle& text_style,
    SelectionPaintState* selection)
    : fragment_paint_info_(fragment_paint_info),
      text_painter_(text_painter),
      decoration_painter_(decoration_painter),
      paint_info_(paint_info),
      cursor_(cursor),
      root_inline_cursor_(cursor),
      fragment_item_(fragment_item),
      box_origin_(box_origin),
      originating_style_(style),
      originating_text_style_(text_style),
      selection_(selection),
      layout_object_(fragment_item_.GetLayoutObject()),
      node_(AssociatedNode(layout_object_)),
      foreground_auto_dark_mode_(
          PaintAutoDarkMode(originating_style_,
                            DarkModeFilter::ElementRole::kForeground)),
      background_auto_dark_mode_(
          PaintAutoDarkMode(originating_style_,
                            DarkModeFilter::ElementRole::kBackground)) {
  root_inline_cursor_.ExpandRootToContainingBlock();

  // Custom highlights and marker-based highlights are defined in terms of
  // DOM ranges in a Text node. Generated text either has no Text node or does
  // not derive its content from the Text node (e.g. ellipsis, soft hyphens).
  // TODO(crbug.com/17528) handle ::first-letter
  if (!fragment_item_.IsGeneratedText()) {
    const auto* text_node = DynamicTo<Text>(node_);
    if (text_node) {
      DocumentMarkerController& controller = node_->GetDocument().Markers();
      if (controller.HasAnyMarkersForText(*text_node)) {
        fragment_dom_offsets_ = GetFragmentDOMOffsets(
            *text_node, fragment_paint_info_.from, fragment_paint_info_.to);
        DCHECK(fragment_dom_offsets_);
        markers_ = controller.ComputeMarkersToPaint(*text_node);
        if (RuntimeEnabledFeatures::SearchTextHighlightPseudoEnabled() &&
            !fragment_item_.IsSvgText()) {
          search_ = controller.MarkersFor(
              *text_node, DocumentMarker::kTextMatch,
              fragment_dom_offsets_->start, fragment_dom_offsets_->end);
        }
        target_ = controller.MarkersFor(
            *text_node, DocumentMarker::kTextFragment,
            fragment_dom_offsets_->start, fragment_dom_offsets_->end);
        spelling_ = controller.MarkersFor(*text_node, DocumentMarker::kSpelling,
                                          fragment_dom_offsets_->start,
                                          fragment_dom_offsets_->end);
        grammar_ = controller.MarkersFor(*text_node, DocumentMarker::kGrammar,
                                         fragment_dom_offsets_->start,
                                         fragment_dom_offsets_->end);
        custom_ = controller.MarkersFor(
            *text_node, DocumentMarker::kCustomHighlight,
            fragment_dom_offsets_->start, fragment_dom_offsets_->end);
      } else if (selection) {
        fragment_dom_offsets_ = GetFragmentDOMOffsets(
            *text_node, fragment_paint_info_.from, fragment_paint_info_.to);
      }
    }
  }

  paint_case_ = ComputePaintCase();

  // |layers_| and |parts_| are only needed when using the full overlay
  // painting algorithm, otherwise we can leave them empty.
  if (paint_case_ == kOverlay) {
    auto* selection_status = GetSelectionStatus(selection_);
    layers_ = HighlightOverlay::ComputeLayers(
        layout_object_->GetDocument(), node_, originating_style_,
        originating_text_style_, paint_info_, selection_status, custom_,
        grammar_, spelling_, target_, search_);
    Vector<HighlightEdge> edges = HighlightOverlay::ComputeEdges(
        node_, fragment_item_.IsGeneratedText(), fragment_dom_offsets_, layers_,
        selection_status, custom_, grammar_, spelling_, target_, search_);
    parts_ =
        HighlightOverlay::ComputeParts(fragment_paint_info_, layers_, edges);

    if (!parts_.empty()) {
      if (const ShapeResultView* shape_result_view =
              fragment_item_->TextShapeResult()) {
        const ShapeResult* shape_result =
            shape_result_view->CreateShapeResult();
        unsigned start_offset = fragment_item_->StartOffset();
        edges_info_.push_back(HighlightEdgeInfo{
            parts_[0].range.from,
            shape_result->CaretPositionForOffset(
                parts_[0].range.from - start_offset, cursor_.CurrentText())});
        for (const HighlightPart& part : parts_) {
          edges_info_.push_back(HighlightEdgeInfo{
              part.range.to,
              shape_result->CaretPositionForOffset(part.range.to - start_offset,
                                                   cursor_.CurrentText())});
        }
      } else {
        edges_info_.push_back(HighlightEdgeInfo{
            parts_[0].range.from,
            fragment_item_
                .CaretInlinePositionForOffset(cursor_.CurrentText(),
                                              parts_[0].range.from)
                .ToFloat()});
        for (const HighlightPart& part : parts_) {
          edges_info_.push_back(HighlightEdgeInfo{
              part.range.to, fragment_item_
                                 .CaretInlinePositionForOffset(
                                     cursor_.CurrentText(), part.range.to)
                                 .ToFloat()});
        }
      }
    }
  }
}

void HighlightPainter::PaintNonCssMarkers(Phase phase) {
  if (markers_.empty())
    return;

  CHECK(node_);
  const StringView text = cursor_.CurrentText();

  const auto* text_node = DynamicTo<Text>(node_);
  const MarkerRangeMappingContext mapping_context(*text_node,
                                                  *fragment_dom_offsets_);
  for (const DocumentMarker* marker : markers_) {
    std::optional<TextOffsetRange> marker_offsets =
        mapping_context.GetTextContentOffsets(*marker);
    if (!marker_offsets || (marker_offsets->start == marker_offsets->end)) {
      continue;
    }
    const unsigned paint_start_offset = marker_offsets->start;
    const unsigned paint_end_offset = marker_offsets->end;

    DCHECK(!DocumentMarker::MarkerTypes::HighlightPseudos().Contains(
        marker->GetType()));

    switch (marker->GetType()) {
      case DocumentMarker::kTextMatch: {
        if (RuntimeEnabledFeatures::SearchTextHighlightPseudoEnabled() &&
            !fragment_item_->IsSvgText()) {
          break;
        }
        const Document& document = node_->GetDocument();
        const auto& text_match_marker = To<TextMatchMarker>(*marker);
        if (phase == kBackground) {
          Color color =
              LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
                  text_match_marker.IsActiveMatch(),
                  document.InForcedColorsMode(),
                  originating_style_.UsedColorScheme(),
                  document.GetColorProviderForPainting(
                      originating_style_.UsedColorScheme()),
                  document.IsInWebAppScope());
          PaintRect(
              paint_info_.context,
              ComputeBackgroundRect(text, paint_start_offset, paint_end_offset),
              color, background_auto_dark_mode_);
          break;
        }

        const TextPaintStyle text_style =
            TextPaintStyleForTextMatch(text_match_marker, originating_style_,
                                       document, fragment_item_->IsSvgText());
        if (fragment_item_->IsSvgText()) {
          text_painter_.SetSvgState(
              *To<LayoutSVGInlineText>(fragment_item_->GetLayoutObject()),
              originating_style_, text_style.fill_color);
        }
        text_painter_.Paint(
            fragment_paint_info_.Slice(paint_start_offset, paint_end_offset),
            text_style, kInvalidDOMNodeId, foreground_auto_dark_mode_);
      } break;

      case DocumentMarker::kComposition:
      case DocumentMarker::kActiveSuggestion:
      case DocumentMarker::kSuggestion: {
        const auto& styleable_marker = To<StyleableMarker>(*marker);
        if (phase == kBackground) {
          PaintRect(
              paint_info_.context,
              ComputeBackgroundRect(text, paint_start_offset, paint_end_offset),
              styleable_marker.BackgroundColor(), background_auto_dark_mode_);
          break;
        }
        if (StyleableMarkerPainter::ShouldPaintUnderline(styleable_marker)) {
          const SimpleFontData* font_data =
              originating_style_.GetFont().PrimaryFont();
          StyleableMarkerPainter::PaintUnderline(
              styleable_marker, paint_info_.context, box_origin_,
              originating_style_,
              LineRelativeLocalRect(fragment_item_, text, paint_start_offset,
                                    paint_end_offset),
              LayoutUnit(font_data->GetFontMetrics().Height()),
              node_->GetDocument().InDarkMode());
        }
        if (marker->GetType() == DocumentMarker::kComposition &&
            !styleable_marker.TextColor().IsFullyTransparent() &&
            RuntimeEnabledFeatures::CompositionForegroundMarkersEnabled()) {
          PaintTextForCompositionMarker(text, styleable_marker.TextColor(),
                                        paint_start_offset, paint_end_offset);
        }
        break;
      }
      case DocumentMarker::kSpelling:
      case DocumentMarker::kGrammar:
      case DocumentMarker::kTextFragment:
      case DocumentMarker::kCustomHighlight:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}

HighlightPainter::Case HighlightPainter::PaintCase() const {
  return paint_case_;
}

HighlightPainter::Case HighlightPainter::ComputePaintCase() const {
  if (selection_ && selection_->ShouldPaintSelectedTextOnly())
    return kSelectionOnly;

  // This can yield false positives (weakening the optimisations below) if all
  // non-spelling/grammar/selection highlights are outside the text fragment.
  if (!target_.empty() || !search_.empty() || !custom_.empty()) {
    return kOverlay;
  }

  if (selection_ && spelling_.empty() && grammar_.empty()) {
    const ComputedStyle* pseudo_style =
        HighlightStyleUtils::HighlightPseudoStyle(node_, originating_style_,
                                                  kPseudoIdSelection);

    // If we only have a selection, and there are no selection or originating
    // decorations, we don’t need the expense of overlay painting.
    return !originating_style_.HasAppliedTextDecorations() &&
                   (!pseudo_style || !pseudo_style->HasAppliedTextDecorations())
               ? kFastSelection
               : kOverlay;
  }

  if (!spelling_.empty() || !grammar_.empty()) {
    // If there is a selection too, we must use the overlay painting algorithm.
    if (selection_)
      return kOverlay;

    // If there are only spelling and/or grammar highlights, and they use the
    // default style that only adds decorations without adding a background or
    // changing the text color, we don’t need the expense of overlay painting.
    bool spelling_ok =
        spelling_.empty() ||
        !HasNonTrivialSpellingGrammarStyles(
            fragment_item_, node_, originating_style_, kPseudoIdSpellingError);
    bool grammar_ok =
        grammar_.empty() ||
        !HasNonTrivialSpellingGrammarStyles(
            fragment_item_, node_, originating_style_, kPseudoIdGrammarError);
    return spelling_ok && grammar_ok ? kFastSpellingGrammar : kOverlay;
  }

  DCHECK(!selection_ && target_.empty() && spelling_.empty() &&
         grammar_.empty() && custom_.empty());
  return kNoHighlights;
}

void HighlightPainter::FastPaintSpellingGrammarDecorations() {
  DCHECK_EQ(paint_case_, kFastSpellingGrammar);
  CHECK(node_);
  const auto& text_node = To<Text>(*node_);
  const StringView text = cursor_.CurrentText();

  // ::spelling-error overlay is drawn on top of ::grammar-error overlay.
  // https://drafts.csswg.org/css-pseudo-4/#highlight-backgrounds
  FastPaintSpellingGrammarDecorations(text_node, text, grammar_);
  FastPaintSpellingGrammarDecorations(text_node, text, spelling_);
}

void HighlightPainter::FastPaintSpellingGrammarDecorations(
    const Text& text_node,
    const StringView& text,
    const DocumentMarkerVector& markers) {
  const MarkerRangeMappingContext mapping_context(text_node,
                                                  *fragment_dom_offsets_);
  for (const DocumentMarker* marker : markers) {
    std::optional<TextOffsetRange> marker_offsets =
        mapping_context.GetTextContentOffsets(*marker);
    if (!marker_offsets || (marker_offsets->start == marker_offsets->end)) {
      continue;
    }
    PaintOneSpellingGrammarDecoration(
        marker->GetType(), text, marker_offsets->start, marker_offsets->end);
  }
}

void HighlightPainter::PaintOneSpellingGrammarDecoration(
    DocumentMarker::MarkerType type,
    const StringView& text,
    unsigned paint_start_offset,
    unsigned paint_end_offset) {
  if (node_->GetDocument().Printing()) {
    return;
  }

  if (!text_painter_.GetSvgState()) {
    if (const auto* pseudo_style = HighlightStyleUtils::HighlightPseudoStyle(
            node_, originating_style_, PseudoFor(type))) {
      const TextPaintStyle text_style =
          HighlightStyleUtils::HighlightPaintingStyle(
              node_->GetDocument(), originating_style_, pseudo_style, node_,
              PseudoFor(type), originating_text_style_, paint_info_,
              SearchTextIsCurrent::kNo)
              .style;
      PaintOneSpellingGrammarDecoration(type, text, paint_start_offset,
                                        paint_end_offset, *pseudo_style,
                                        text_style, nullptr);
      return;
    }
  }

  // If they are not yet implemented (as is the case for SVG), or they have no
  // styles (as there can be for non-HTML content or for HTML content with the
  // wrong root), use the originating style with the decorations override set
  // to a synthesised AppliedTextDecoration.
  //
  // For the synthesised decoration, just like with our real spelling/grammar
  // decorations, the ‘text-decoration-style’, ‘text-decoration-thickness’, and
  // ‘text-underline-offset’ are irrelevant.
  //
  // SVG painting currently ignores ::selection styles, and will malfunction
  // or crash if asked to paint decorations introduced by highlight pseudos.
  // TODO(crbug.com/1147859) is SVG spec ready for highlight decorations?
  // TODO(crbug.com/1147859) https://github.com/w3c/svgwg/issues/894
  const AppliedTextDecoration synthesised{
      LineFor(type), {}, ColorFor(type), {}, {}};
  PaintOneSpellingGrammarDecoration(type, text, paint_start_offset,
                                    paint_end_offset, originating_style_,
                                    originating_text_style_, &synthesised);
}

void HighlightPainter::PaintOneSpellingGrammarDecoration(
    DocumentMarker::MarkerType marker_type,
    const StringView& text,
    unsigned paint_start_offset,
    unsigned paint_end_offset,
    const ComputedStyle& style,
    const TextPaintStyle& text_style,
    const AppliedTextDecoration* decoration_override) {
  // When painting decorations on the spelling/grammar fast path, the part and
  // the decoration have the same range, so we can use the same rect for both
  // clipping the canvas and painting the decoration.
  const HighlightRange range{paint_start_offset, paint_end_offset};
  const LineRelativeRect rect = LineRelativeWorldRect(range);

  std::optional<TextDecorationInfo> decoration_info{};
  decoration_painter_.UpdateDecorationInfo(decoration_info, fragment_item_,
                                           style, rect, decoration_override);

  GraphicsContextStateSaver saver{paint_info_.context};
  ClipToPartRect(rect);

  decoration_painter_.PaintExceptLineThrough(
      *decoration_info, text_style,
      fragment_paint_info_.Slice(paint_start_offset, paint_end_offset),
      LineFor(marker_type));
}

void HighlightPainter::PaintOriginatingShadow(const TextPaintStyle& text_style,
                                              DOMNodeId node_id) {
  DCHECK_EQ(paint_case_, kOverlay);

  // First paint the shadows for the whole range.
  if (text_style.shadow) {
    text_painter_.Paint(fragment_paint_info_, text_style, node_id,
                        foreground_auto_dark_mode_, TextPainter::kShadowsOnly);
  }
}

Vector<LayoutSelectionStatus> HighlightPainter::GetHighlights(
    const HighlightLayer& layer) {
  Vector<LayoutSelectionStatus> result{};
  const auto* text_node = DynamicTo<Text>(node_);
  switch (layer.type) {
    case HighlightLayerType::kOriginating:
      NOTREACHED_IN_MIGRATION();
      break;
    case HighlightLayerType::kCustom: {
      DCHECK(text_node);
      const MarkerRangeMappingContext mapping_context(*text_node,
                                                      *fragment_dom_offsets_);
      for (const auto& marker : custom_) {
        // Filter custom highlight markers to one highlight at a time.
        auto* custom = To<CustomHighlightMarker>(marker.Get());
        if (custom->GetHighlightName() != layer.PseudoArgument()) {
          continue;
        }
        std::optional<TextOffsetRange> marker_offsets =
            mapping_context.GetTextContentOffsets(*marker);
        if (marker_offsets && (marker_offsets->start != marker_offsets->end)) {
          result.push_back(
              LayoutSelectionStatus{marker_offsets->start, marker_offsets->end,
                                    SelectSoftLineBreak::kNotSelected});
        }
      }
      break;
    }
    case HighlightLayerType::kGrammar: {
      DCHECK(text_node);
      const MarkerRangeMappingContext mapping_context(*text_node,
                                                      *fragment_dom_offsets_);
      for (const auto& marker : grammar_) {
        std::optional<TextOffsetRange> marker_offsets =
            mapping_context.GetTextContentOffsets(*marker);
        if (marker_offsets && (marker_offsets->start != marker_offsets->end)) {
          result.push_back(
              LayoutSelectionStatus{marker_offsets->start, marker_offsets->end,
                                    SelectSoftLineBreak::kNotSelected});
        }
      }
      break;
    }
    case HighlightLayerType::kSpelling: {
      DCHECK(text_node);
      const MarkerRangeMappingContext mapping_context(*text_node,
                                                      *fragment_dom_offsets_);
      for (const auto& marker : spelling_) {
        std::optional<TextOffsetRange> marker_offsets =
            mapping_context.GetTextContentOffsets(*marker);
        if (marker_offsets && (marker_offsets->start != marker_offsets->end)) {
          result.push_back(
              LayoutSelectionStatus{marker_offsets->start, marker_offsets->end,
                                    SelectSoftLineBreak::kNotSelected});
        }
      }
      break;
    }
    case HighlightLayerType::kTargetText: {
      DCHECK(text_node);
      const MarkerRangeMappingContext mapping_context(*text_node,
                                                      *fragment_dom_offsets_);
      for (const auto& marker : target_) {
        std::optional<TextOffsetRange> marker_offsets =
            mapping_context.GetTextContentOffsets(*marker);
        if (marker_offsets && (marker_offsets->start != marker_offsets->end)) {
          result.push_back(
              LayoutSelectionStatus{marker_offsets->start, marker_offsets->end,
                                    SelectSoftLineBreak::kNotSelected});
        }
      }
      break;
    }
    case HighlightLayerType::kSearchText:
    case HighlightLayerType::kSearchTextCurrent: {
      DCHECK(text_node);
      const MarkerRangeMappingContext mapping_context(*text_node,
                                                      *fragment_dom_offsets_);
      for (const auto& marker : search_) {
        auto* text_match_marker = To<TextMatchMarker>(marker.Get());
        bool is_current = layer.type == HighlightLayerType::kSearchTextCurrent;
        if (text_match_marker->IsActiveMatch() != is_current) {
          continue;
        }
        std::optional<TextOffsetRange> marker_offsets =
            mapping_context.GetTextContentOffsets(*marker);
        if (marker_offsets && (marker_offsets->start != marker_offsets->end)) {
          result.push_back(
              LayoutSelectionStatus{marker_offsets->start, marker_offsets->end,
                                    SelectSoftLineBreak::kNotSelected});
        }
      }
      break;
    }
    case HighlightLayerType::kSelection:
      result.push_back(*GetSelectionStatus(selection_));
      break;
  }
  return result;
}

TextOffsetRange HighlightPainter::GetFragmentDOMOffsets(const Text& text,
                                                        unsigned from,
                                                        unsigned to) {
  const OffsetMapping* mapping = OffsetMapping::GetFor(text.GetLayoutObject());
  unsigned last_from = mapping->GetLastPosition(from).OffsetInContainerNode();
  unsigned first_to = mapping->GetFirstPosition(to).OffsetInContainerNode();
  return {last_from, first_to};
}

const PhysicalRect HighlightPainter::ComputeBackgroundRect(
    StringView text,
    unsigned start_offset,
    unsigned end_offset) {
  return fragment_item_.LocalRect(text, start_offset, end_offset) + box_origin_;
}

const PhysicalRect HighlightPainter::ComputeBackgroundRectForSelection(
    unsigned start_offset,
    unsigned end_offset) {
  LayoutSelectionStatus selection_status{selection_->Status()};
  selection_status.start = start_offset;
  selection_status.end = end_offset;
  return root_inline_cursor_.CurrentLocalSelectionRectForText(
             selection_status) +
         box_origin_;
}

void HighlightPainter::PaintHighlightOverlays(
    const TextPaintStyle& originating_text_style,
    DOMNodeId node_id,
    bool paint_marker_backgrounds,
    std::optional<AffineTransform> rotation) {
  DCHECK_EQ(paint_case_, kOverlay);

  // |node_| might not be a Text node (e.g. <br>), or it might be nullptr (e.g.
  // ::first-letter). In both cases, we should still try to paint kOriginating
  // and kSelection if necessary, but we can’t paint marker-based highlights,
  // because GetTextContentOffset requires a Text node. Markers are defined and
  // stored in terms of Text nodes anyway, so this should never be a problem.

  // For each overlay, paint ‘background-color’ and ‘text-shadow’ one part at a
  // time, since both of them may vary in color when ‘color’ is ‘currentColor’.
  for (wtf_size_t i = 0; i < layers_.size(); i++) {
    const HighlightLayer& layer = layers_[i];

    if (layer.type == HighlightLayerType::kOriginating) {
      continue;
    }

    if (layer.type == HighlightLayerType::kSelection &&
        !paint_marker_backgrounds) {
      continue;
    }

    // Paint ‘background-color’ while trying to merge parts if possible,
    // to avoid creating unnecessary “seams”.
    MergedHighlightPart<HighlightBackground> merged_background{};
    const auto& paint_background =
        [&](const MergedHighlightPart<HighlightBackground>::Merged& merged) {
          if (merged.inner.color.IsFullyTransparent()) {
            return;
          }
          // TODO(crbug.com/40281215) ComputeBackgroundRect should use the same
          // logic as ComputeBackgroundRectForSelection, that is, it should
          // expand selection to the line height and extend for line breaks.
          PhysicalRect part_rect =
              layer.type == HighlightLayerType::kSelection
                  ? ComputeBackgroundRectForSelection(merged.from, merged.to)
                  : ComputeBackgroundRect(cursor_.CurrentText(), merged.from,
                                          merged.to);
          PaintHighlightBackground(paint_info_.context, originating_style_,
                                   merged.inner.color, part_rect, rotation);
        };
    for (const HighlightPart& part : parts_) {
      for (const HighlightBackground& background : part.backgrounds) {
        if (background.layer_index == i) {
          if (const auto& merged = merged_background.Merge(background, part)) {
            paint_background(*merged);
          }
          break;
        }
      }
    }
    if (const auto& merged = merged_background.Take()) {
      paint_background(*merged);
    }

    // Paint ‘text-shadow’ while trying to merge parts if possible,
    // to avoid unnecessarily splitting ligatures.
    MergedHighlightPart<HighlightTextShadow> merged_text_shadow{};
    const auto& paint_text_shadow =
        [&](const MergedHighlightPart<HighlightTextShadow>::Merged& merged) {
          if (!layer.text_style.style.shadow) {
            return;
          }
          TextPaintStyle text_shadow_style{};
          text_shadow_style.shadow = layer.text_style.style.shadow;
          text_shadow_style.current_color = merged.inner.current_color;
          text_painter_.Paint(
              fragment_paint_info_.Slice(merged.from, merged.to),
              text_shadow_style, node_id, foreground_auto_dark_mode_,
              TextPainter::kShadowsOnly);
        };
    for (const HighlightPart& part : parts_) {
      for (const HighlightTextShadow& text_shadow : part.text_shadows) {
        if (text_shadow.layer_index == i) {
          if (const auto& merged =
                  merged_text_shadow.Merge(text_shadow, part)) {
            paint_text_shadow(*merged);
          }
          break;
        }
      }
    }
    if (const auto& merged = merged_text_shadow.Take()) {
      paint_text_shadow(*merged);
    }
  }

  // For each part, paint the text proper over every highlighted range,
  for (auto& part : parts_) {
    LineRelativeRect part_rect = LineRelativeWorldRect(part.range);

    PaintDecorationsExceptLineThrough(part, part_rect);

    // Only paint text if we have a shape result. See TextPainter::Paint().
    if (fragment_paint_info_.shape_result) {
      std::optional<base::AutoReset<bool>> is_painting_selection_reset;
      GraphicsContextStateSaver state_saver(paint_info_.context);
      // SVG text may have transforms that defeat clipping. The clipping
      // is only required for ligatures, so we will accept potential
      // double painting of ligatures for SVG so as to correctly handle
      // transformed text (include text paths). This might be fixable by
      // transforming the ink overflow before using it to expamd the clip.
      TextPainter::SvgTextPaintState* svg_state = text_painter_.GetSvgState();
      if (svg_state && part.type == HighlightLayerType::kSelection)
          [[unlikely]] {
        // SVG text painting needs to know it is painting selection.
        is_painting_selection_reset.emplace(&svg_state->is_painting_selection_,
                                            true);
      } else {
        LineRelativeRect clip_rect = part_rect;
        if (part.stroke_width > 0) {
          clip_rect.Inflate(
              LayoutUnit::FromFloatCeil(part.stroke_width / 2.0f));
        }
        // If we're at the far left or right end of a fragment, expand the clip
        // to avoid clipping characters (italics and some antialiasing).
        if (part.range.from == fragment_paint_info_.from) {
          clip_rect.AdjustLineStartToInkOverflow(fragment_item_);
        }
        if (part.range.to == fragment_paint_info_.to) {
          clip_rect.AdjustLineEndToInkOverflow(fragment_item_);
        }
        ClipToPartRect(clip_rect.EnclosingLineRelativeRect());
      }

      // Adjust start/end offset when they are in the middle of a ligature.
      // e.g., when |start_offset| is between a ligature of "fi", it needs to
      // be adjusted to before "f".
      unsigned start = part.range.from;
      unsigned end = part.range.to;
      fragment_paint_info_.shape_result->ExpandRangeToIncludePartialGlyphs(
          &start, &end);

      text_painter_.Paint(fragment_paint_info_.Slice(start, end),
                          part.style, node_id, foreground_auto_dark_mode_,
                          TextPainter::kTextProperOnly);
    }

    PaintDecorationsOnlyLineThrough(part, part_rect);
  }
}

void HighlightPainter::PaintHighlightBackground(
    GraphicsContext& context,
    const ComputedStyle& style,
    Color color,
    const PhysicalRect& rect,
    const std::optional<AffineTransform>& rotation) {
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kSelection));

  if (!rotation) {
    PaintRect(context, rect, color, auto_dark_mode);
    return;
  }

  // PaintRect tries to pixel-snap the given rect, but if we’re painting in a
  // non-horizontal writing mode, our context has been transformed, regressing
  // tests like <paint/invalidation/repaint-across-writing-mode-boundary>. To
  // fix this, we undo the transformation temporarily, then use the original
  // physical coordinates (before MapSelectionRectIntoRotatedSpace).
  context.ConcatCTM(rotation->Inverse());
  PaintRect(context, rect, color, auto_dark_mode);
  context.ConcatCTM(*rotation);
}

PseudoId HighlightPainter::PseudoFor(DocumentMarker::MarkerType type) {
  switch (type) {
    case DocumentMarker::kSpelling:
      return kPseudoIdSpellingError;
    case DocumentMarker::kGrammar:
      return kPseudoIdGrammarError;
    case DocumentMarker::kTextFragment:
      return kPseudoIdTargetText;
    default:
      NOTREACHED_IN_MIGRATION();
      return {};
  }
}

TextDecorationLine HighlightPainter::LineFor(DocumentMarker::MarkerType type) {
  switch (type) {
    case DocumentMarker::kSpelling:
      return TextDecorationLine::kSpellingError;
    case DocumentMarker::kGrammar:
      return TextDecorationLine::kGrammarError;
    default:
      NOTREACHED_IN_MIGRATION();
      return {};
  }
}

Color HighlightPainter::ColorFor(DocumentMarker::MarkerType type) {
  switch (type) {
    case DocumentMarker::kSpelling:
      return LayoutTheme::GetTheme().PlatformSpellingMarkerUnderlineColor();
    case DocumentMarker::kGrammar:
      return LayoutTheme::GetTheme().PlatformGrammarMarkerUnderlineColor();
    default:
      NOTREACHED_IN_MIGRATION();
      return {};
  }
}

LineRelativeRect HighlightPainter::LineRelativeWorldRect(
    const HighlightOverlay::HighlightRange& range) {
  return LocalRectInWritingModeSpace(range.from, range.to) +
         LineRelativeOffset::CreateFromBoxOrigin(box_origin_);
}

LineRelativeRect HighlightPainter::LocalRectInWritingModeSpace(
    unsigned from,
    unsigned to) const {
  if (paint_case_ != kOverlay) {
    const StringView text = cursor_.CurrentText();
    return LineRelativeLocalRect(fragment_item_, text, from, to);
  }

  auto from_info =
      std::lower_bound(edges_info_.begin(), edges_info_.end(), from,
                       [](const HighlightEdgeInfo& info, unsigned offset) {
                         return info.offset < offset;
                       });
  auto to_info =
      std::lower_bound(from_info, edges_info_.end(), to,
                       [](const HighlightEdgeInfo& info, unsigned offset) {
                         return info.offset < offset;
                       });
  CHECK_NE(from_info, edges_info_.end(), base::NotFatalUntil::M130);
  CHECK_NE(to_info, edges_info_.end(), base::NotFatalUntil::M130);

  // This rect is used for 2 purposes: To set the offset and width for
  // text decoration painting, and the set the clip. The former uses the
  // offset and width, but not height, and the offset should be the
  // fragment offset. The latter uses offset and size, but the offset should
  // be the corner of the painted region. Return the origin for text decorations
  // and the height for clipping, then update the offset for clipping in the
  // calling code.
  const LayoutUnit height = fragment_item_.InkOverflowRect().Height();
  LayoutUnit left;
  LayoutUnit right;
  if (from_info->x > to_info->x) {
    left = LayoutUnit::FromFloatFloor(to_info->x);
    right = LayoutUnit::FromFloatCeil(from_info->x);
  } else {
    left = LayoutUnit::FromFloatFloor(from_info->x);
    right = LayoutUnit::FromFloatCeil(to_info->x);
  }
  return {{left, LayoutUnit{}}, {right - left, height}};
}

void HighlightPainter::ClipToPartRect(const LineRelativeRect& part_rect) {
  gfx::RectF clip_rect{part_rect};
  if (fragment_item_.IsSvgText()) [[unlikely]] {
    clip_rect = TextDecorationPainter::ExpandRectForSVGDecorations(part_rect);
  } else {
    clip_rect.Offset(0, fragment_item_.InkOverflowRect().Y());
  }
  paint_info_.context.Clip(clip_rect);
}

void HighlightPainter::PaintDecorationsExceptLineThrough(
    const HighlightPart& part,
    const LineRelativeRect& part_rect) {
  // Line decorations in highlight pseudos are ordered first by the kind of line
  // (underlines before overlines), then by the highlight layer they came from.
  // https://github.com/w3c/csswg-drafts/issues/6022
  PaintDecorationsExceptLineThrough(part, part_rect,
                                    TextDecorationLine::kUnderline);
  PaintDecorationsExceptLineThrough(part, part_rect,
                                    TextDecorationLine::kOverline);
  PaintDecorationsExceptLineThrough(
      part, part_rect,
      TextDecorationLine::kSpellingError | TextDecorationLine::kGrammarError);
}

void HighlightPainter::PaintDecorationsExceptLineThrough(
    const HighlightPart& part,
    const LineRelativeRect& part_rect,
    TextDecorationLine lines_to_paint) {
  GraphicsContextStateSaver state_saver(paint_info_.context, false);
  for (const HighlightDecoration& decoration : part.decorations) {
    HighlightLayer& decoration_layer = layers_[decoration.layer_index];

    // Clipping the canvas unnecessarily is expensive, so avoid doing it if
    // there are no decorations of the given |lines_to_paint|.
    if (!EnumHasFlags(decoration_layer.decorations_in_effect, lines_to_paint)) {
      continue;
    }

    // SVG painting currently ignores ::selection styles, and will malfunction
    // or crash if asked to paint decorations introduced by highlight pseudos.
    // TODO(crbug.com/1147859) is SVG spec ready for highlight decorations?
    // TODO(crbug.com/1147859) https://github.com/w3c/svgwg/issues/894
    if (text_painter_.GetSvgState() &&
        decoration.type != HighlightLayerType::kOriginating) {
      continue;
    }

    // Paint the decoration over the range of the originating fragment or active
    // highlight, but clip it to the range of the part.
    const LineRelativeRect decoration_rect =
        LineRelativeWorldRect(decoration.range);

    std::optional<TextDecorationInfo> decoration_info{};
    decoration_painter_.UpdateDecorationInfo(decoration_info, fragment_item_,
                                             *decoration_layer.style,
                                             decoration_rect);

    if (part.type != HighlightLayerType::kOriginating) {
      if (decoration.type == HighlightLayerType::kOriginating) {
        decoration_info->SetHighlightOverrideColor(part.style.current_color);
      } else {
        decoration_info->SetHighlightOverrideColor(
            decoration.highlight_override_color);
      }
    }

    if (!state_saver.Saved()) {
      state_saver.Save();
      const LineRelativeRect clip_rect =
          part.range != decoration.range ? part_rect : decoration_rect;
      ClipToPartRect(clip_rect);
    }

    decoration_painter_.PaintExceptLineThrough(
        *decoration_info, decoration_layer.text_style.style,
        fragment_paint_info_.Slice(part.range.from, part.range.to),
        lines_to_paint);
  }
}

void HighlightPainter::PaintDecorationsOnlyLineThrough(
    const HighlightPart& part,
    const LineRelativeRect& part_rect) {
  GraphicsContextStateSaver state_saver(paint_info_.context, false);
  for (const HighlightDecoration& decoration : part.decorations) {
    HighlightLayer& decoration_layer = layers_[decoration.layer_index];

    // Clipping the canvas unnecessarily is expensive, so avoid doing it if
    // there are no ‘line-through’ decorations.
    if (!EnumHasFlags(decoration_layer.decorations_in_effect,
                      TextDecorationLine::kLineThrough)) {
      continue;
    }

    // SVG painting currently ignores ::selection styles, and will malfunction
    // or crash if asked to paint decorations introduced by highlight pseudos.
    // TODO(crbug.com/1147859) is SVG spec ready for highlight decorations?
    // TODO(crbug.com/1147859) https://github.com/w3c/svgwg/issues/894
    if (text_painter_.GetSvgState() &&
        decoration.type != HighlightLayerType::kOriginating) {
      continue;
    }

    // Paint the decoration over the range of the originating fragment or active
    // highlight, but clip it to the range of the part.
    const LineRelativeRect decoration_rect =
        LineRelativeWorldRect(decoration.range);

    std::optional<TextDecorationInfo> decoration_info{};
    decoration_painter_.UpdateDecorationInfo(decoration_info, fragment_item_,
                                             *decoration_layer.style,
                                             decoration_rect);

    if (part.type != HighlightLayerType::kOriginating) {
      if (decoration.type == HighlightLayerType::kOriginating) {
        decoration_info->SetHighlightOverrideColor(part.style.current_color);
      } else {
        decoration_info->SetHighlightOverrideColor(
            decoration.highlight_override_color);
      }
    }

    if (!state_saver.Saved()) {
      state_saver.Save();
      const LineRelativeRect clip_rect =
          part.range != decoration.range ? part_rect : decoration_rect;
      ClipToPartRect(clip_rect);
    }

    decoration_painter_.PaintOnlyLineThrough(*decoration_info,
                                             decoration_layer.text_style.style);
  }
}

void HighlightPainter::PaintTextForCompositionMarker(
    const StringView& text,
    const Color& text_color,
    unsigned paint_start_offset,
    unsigned paint_end_offset) {
  TextPaintStyle text_style;
  text_style.current_color = text_style.fill_color = text_style.stroke_color =
      text_style.emphasis_mark_color = text_color;
  text_style.stroke_width = originating_style_.TextStrokeWidth();
  text_style.color_scheme = originating_style_.UsedColorScheme();
  text_style.shadow = nullptr;
  text_style.paint_order = originating_style_.PaintOrder();

  LineRelativeRect decoration_rect = LineRelativeLocalRect(
      fragment_item_, text, paint_start_offset, paint_end_offset);
  decoration_rect.Move(LineRelativeOffset::CreateFromBoxOrigin(box_origin_));
  TextDecorationPainter decoration_painter(
      text_painter_, decoration_painter_.InlineContext(), paint_info_,
      originating_style_, text_style, decoration_rect, selection_);

  decoration_painter.Begin(fragment_item_, TextDecorationPainter::kOriginating);
  decoration_painter.PaintExceptLineThrough(
      fragment_paint_info_.Slice(paint_start_offset, paint_end_offset));

  text_painter_.Paint(
      fragment_paint_info_.Slice(paint_start_offset, paint_end_offset),
      text_style, kInvalidDOMNodeId, foreground_auto_dark_mode_);

  decoration_painter.PaintOnlyLineThrough();
}

}  // namespace blink
