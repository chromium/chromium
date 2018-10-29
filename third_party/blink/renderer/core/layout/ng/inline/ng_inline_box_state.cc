// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_box_state.h"

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

NGLineHeightMetrics ComputeEmphasisMarkOutsets(const ComputedStyle& style) {
  if (style.GetTextEmphasisMark() == TextEmphasisMark::kNone)
    return NGLineHeightMetrics();

  const Font& font = style.GetFont();
  LayoutUnit emphasis_mark_height =
      LayoutUnit(font.EmphasisMarkHeight(style.TextEmphasisMarkString()));
  DCHECK_GT(emphasis_mark_height, LayoutUnit());
  return style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver
             ? NGLineHeightMetrics(emphasis_mark_height, LayoutUnit())
             : NGLineHeightMetrics(LayoutUnit(), emphasis_mark_height);
}

}  // namespace

void NGInlineBoxState::ComputeTextMetrics(const ComputedStyle& style,
                                          FontBaseline baseline_type) {
  text_metrics = NGLineHeightMetrics(style, baseline_type);
  text_top = -text_metrics.ascent;
  text_height = text_metrics.LineHeight();

  NGLineHeightMetrics emphasis_marks_outsets =
      ComputeEmphasisMarkOutsets(style);
  if (emphasis_marks_outsets.IsEmpty()) {
    text_metrics.AddLeading(style.ComputedLineHeightAsFixed());
  } else {
    NGLineHeightMetrics emphasis_marks_metrics = text_metrics;
    emphasis_marks_metrics += emphasis_marks_outsets;
    text_metrics.AddLeading(style.ComputedLineHeightAsFixed());
    text_metrics.Unite(emphasis_marks_metrics);
    // TODO: Is this correct to include into text_metrics? How do we use
    // text_metrics after this point?
  }

  metrics.Unite(text_metrics);

  include_used_fonts = style.LineHeight().IsNegative();
}

void NGInlineBoxState::ResetTextMetrics() {
  metrics = text_metrics = NGLineHeightMetrics();
  text_top = text_height = LayoutUnit();
}

void NGInlineBoxState::EnsureTextMetrics(const ComputedStyle& style,
                                         FontBaseline baseline_type) {
  if (text_metrics.IsEmpty())
    ComputeTextMetrics(style, baseline_type);
}

void NGInlineBoxState::AccumulateUsedFonts(const ShapeResult* shape_result,
                                           FontBaseline baseline_type) {
  HashSet<const SimpleFontData*> fallback_fonts;
  shape_result->FallbackFonts(&fallback_fonts);
  for (const SimpleFontData* const fallback_font : fallback_fonts) {
    NGLineHeightMetrics fallback_metrics(fallback_font->GetFontMetrics(),
                                         baseline_type);
    fallback_metrics.AddLeading(
        fallback_font->GetFontMetrics().FixedLineSpacing());
    metrics.Unite(fallback_metrics);
  }
}

bool NGInlineBoxState::CanAddTextOfStyle(
    const ComputedStyle& text_style) const {
  if (text_style.VerticalAlign() != EVerticalAlign::kBaseline)
    return false;
  DCHECK(style);
  if (style == &text_style || &style->GetFont() == &text_style.GetFont() ||
      style->GetFont().PrimaryFont() == text_style.GetFont().PrimaryFont())
    return true;
  return false;
}

LayoutObject*
NGInlineLayoutStateStack::ContainingLayoutObjectForAbsolutePositionObjects()
    const {
  for (unsigned i = stack_.size(); i-- > 1;) {
    const NGInlineBoxState& box = stack_[i];
    DCHECK(box.style);
    if (box.style->CanContainAbsolutePositionObjects()) {
      DCHECK(box.item->GetLayoutObject());
      return box.item->GetLayoutObject();
    }
  }
  return nullptr;
}

NGInlineBoxState* NGInlineLayoutStateStack::OnBeginPlaceItems(
    const ComputedStyle* line_style,
    FontBaseline baseline_type,
    bool line_height_quirk) {
  if (stack_.IsEmpty()) {
    // For the first line, push a box state for the line itself.
    stack_.resize(1);
    NGInlineBoxState* box = &stack_.back();
    box->fragment_start = 0;
  } else {
    // For the following lines, clear states that are not shared across lines.
    for (NGInlineBoxState& box : stack_) {
      box.fragment_start = 0;
      if (!line_height_quirk)
        box.metrics = box.text_metrics;
      else
        box.ResetTextMetrics();
      if (box.has_start_edge) {
        // Existing box states are wrapped before they were closed, and hence
        // they do not have start edges, unless 'box-decoration-break: clone'.
        box.has_start_edge =
            box.needs_box_fragment &&
            box.style->BoxDecorationBreak() == EBoxDecorationBreak::kClone;
      }
      DCHECK(box.pending_descendants.IsEmpty());
    }
  }

  DCHECK(box_data_list_.IsEmpty());

  // Initialize the box state for the line box.
  NGInlineBoxState& line_box = LineBoxState();
  if (line_box.style != line_style) {
    line_box.style = line_style;

    // Use a "strut" (a zero-width inline box with the element's font and
    // line height properties) as the initial metrics for the line box.
    // https://drafts.csswg.org/css2/visudet.html#strut
    if (!line_height_quirk)
      line_box.ComputeTextMetrics(*line_style, baseline_type);
  }

  return &stack_.back();
}

NGInlineBoxState* NGInlineLayoutStateStack::OnOpenTag(
    const NGInlineItem& item,
    const NGInlineItemResult& item_result,
    const NGLineBoxFragmentBuilder::ChildList& line_box) {
  DCHECK(item.Style());
  NGInlineBoxState* box = OnOpenTag(*item.Style(), line_box);
  box->item = &item;

  if (item.ShouldCreateBoxFragment()) {
    box->SetNeedsBoxFragment(
        ContainingLayoutObjectForAbsolutePositionObjects());
  }

  // Compute box properties regardless of needs_box_fragment since close tag may
  // also set needs_box_fragment.
  box->has_start_edge = item_result.has_edge;
  box->margin_inline_start = item_result.margins.inline_start;
  box->margin_inline_end = item_result.margins.inline_end;
  box->borders = item_result.borders;
  box->padding = item_result.padding;
  return box;
}

NGInlineBoxState* NGInlineLayoutStateStack::OnOpenTag(
    const ComputedStyle& style,
    const NGLineBoxFragmentBuilder::ChildList& line_box) {
  stack_.resize(stack_.size() + 1);
  NGInlineBoxState* box = &stack_.back();
  box->fragment_start = line_box.size();
  box->style = &style;
  return box;
}

NGInlineBoxState* NGInlineLayoutStateStack::OnCloseTag(
    NGLineBoxFragmentBuilder::ChildList* line_box,
    NGInlineBoxState* box,
    FontBaseline baseline_type,
    bool has_end_edge) {
  DCHECK_EQ(box, &stack_.back());
  box->has_end_edge = has_end_edge;
  EndBoxState(box, line_box, baseline_type);
  // TODO(kojii): When the algorithm restarts from a break token, the stack may
  // underflow. We need either synthesize a missing box state, or push all
  // parents on initialize.
  stack_.pop_back();
  return &stack_.back();
}

void NGInlineLayoutStateStack::OnEndPlaceItems(
    NGLineBoxFragmentBuilder::ChildList* line_box,
    FontBaseline baseline_type) {
  for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
    NGInlineBoxState* box = &(*it);
    if (!box->has_end_edge && box->needs_box_fragment &&
        box->style->BoxDecorationBreak() == EBoxDecorationBreak::kClone)
      box->has_end_edge = true;
    EndBoxState(box, line_box, baseline_type);
  }
}

void NGInlineLayoutStateStack::EndBoxState(
    NGInlineBoxState* box,
    NGLineBoxFragmentBuilder::ChildList* line_box,
    FontBaseline baseline_type) {
  if (box->needs_box_fragment)
    AddBoxFragmentPlaceholder(box, line_box, baseline_type);

  PositionPending position_pending =
      ApplyBaselineShift(box, line_box, baseline_type);

  // We are done here if there is no parent box.
  if (box == stack_.begin())
    return;
  NGInlineBoxState& parent_box = *std::prev(box);

  // Propagate necessary data back to the parent box.

  // Unite the metrics to the parent box.
  if (position_pending == kPositionNotPending)
    parent_box.metrics.Unite(box->metrics);
}

void NGInlineBoxState::SetNeedsBoxFragment(
    const LayoutObject* inline_container) {
  DCHECK(item);
  DCHECK(!needs_box_fragment);
  needs_box_fragment = true;
  DCHECK(!this->inline_container);
  this->inline_container = inline_container;
}

// Crete a placeholder for a box fragment.
// We keep a flat list of fragments because it is more suitable for operations
// such as ApplyBaselineShift. Later, CreateBoxFragments() creates box fragments
// from placeholders.
void NGInlineLayoutStateStack::AddBoxFragmentPlaceholder(
    NGInlineBoxState* box,
    NGLineBoxFragmentBuilder::ChildList* line_box,
    FontBaseline baseline_type) {
  DCHECK(box->needs_box_fragment);

  // The inline box should have the height of the font metrics without the
  // line-height property. Compute from style because |box->metrics| includes
  // the line-height property.
  DCHECK(box->style);
  const ComputedStyle& style = *box->style;
  NGLineHeightMetrics metrics(style, baseline_type);

  // Extend the block direction of the box by borders and paddings. Inline
  // direction is already included into positions in NGLineBreaker.
  NGLogicalOffset offset(
      LayoutUnit(),
      -metrics.ascent - (box->borders.line_over + box->padding.line_over));
  NGLogicalSize size(
      LayoutUnit(),
      metrics.LineHeight() + box->borders.BlockSum() + box->padding.BlockSum());

  unsigned fragment_end = line_box->size();
  DCHECK(box->item);
  box_data_list_.push_back(
      BoxData{box->fragment_start, fragment_end, box->item, size});
  BoxData& box_data = box_data_list_.back();
  box_data.padding = box->padding;
  box_data.inline_container = box->inline_container;
  if (box->has_start_edge) {
    box_data.has_line_left_edge = true;
    box_data.margin_line_left = box->margin_inline_start;
    box_data.margin_border_padding_line_left = box->margin_inline_start +
                                               box->borders.inline_start +
                                               box->padding.inline_start;
  }
  if (box->has_end_edge) {
    box_data.has_line_right_edge = true;
    box_data.margin_line_right = box->margin_inline_end;
    box_data.margin_border_padding_line_right = box->margin_inline_end +
                                                box->borders.inline_end +
                                                box->padding.inline_end;
  }
  if (IsRtl(style.Direction())) {
    std::swap(box_data.has_line_left_edge, box_data.has_line_right_edge);
    std::swap(box_data.margin_line_left, box_data.margin_line_right);
    std::swap(box_data.margin_border_padding_line_left,
              box_data.margin_border_padding_line_right);
  }

  if (fragment_end > box->fragment_start) {
    // The start is marked only in BoxData, while end is marked
    // in both BoxData and the list itself.
    // With a list of 4 text fragments:
    // |  0  |  1  |  2  |  3  |
    // |text0|text1|text2|text3|
    // By adding a BoxData(2,4) (end is exclusive), it becomes:
    // |  0  |  1  |  2  |  3  |  4  |
    // |text0|text1|text2|text3|null |
    // The "null" is added to the list to compute baseline shift of the box
    // separately from text fragments.
    line_box->AddChild(offset);
  } else {
    // Do not defer creating a box fragment if this is an empty inline box.
    // An empty box fragment is still flat that we do not have to defer.
    // Also, placeholders cannot be reordred if empty.
    offset.inline_offset += box_data.margin_line_left;
    LayoutUnit advance = box_data.margin_border_padding_line_left +
                         box_data.margin_border_padding_line_right;
    box_data.size.inline_size =
        advance - box_data.margin_line_left - box_data.margin_line_right;
    line_box->AddChild(box_data.CreateBoxFragment(line_box), offset, advance,
                       0);
    box_data_list_.pop_back();
  }
}

void NGInlineLayoutStateStack::PrepareForReorder(
    NGLineBoxFragmentBuilder::ChildList* line_box) {
  // Set indexes of BoxData to the children of the line box.
  unsigned box_data_index = 0;
  for (const BoxData& box_data : box_data_list_) {
    box_data_index++;
    for (unsigned i = box_data.fragment_start; i < box_data.fragment_end; i++) {
      NGLineBoxFragmentBuilder::Child& child = (*line_box)[i];
      if (!child.box_data_index)
        child.box_data_index = box_data_index;
    }
  }

  // When boxes are nested, placeholders have indexes to which box it should be
  // added. Copy them to BoxData.
  for (BoxData& box_data : box_data_list_) {
    const NGLineBoxFragmentBuilder::Child& placeholder =
        (*line_box)[box_data.fragment_end];
    DCHECK(!placeholder.HasFragment());
    box_data.offset = placeholder.offset;
    box_data.box_data_index = placeholder.box_data_index;
  }
}

void NGInlineLayoutStateStack::UpdateAfterReorder(
    NGLineBoxFragmentBuilder::ChildList* line_box) {
  // Compute start/end of boxes from the children of the line box.
  for (BoxData& box_data : box_data_list_)
    box_data.fragment_start = box_data.fragment_end = 0;
  for (unsigned i = 0; i < line_box->size(); i++) {
    const NGLineBoxFragmentBuilder::Child& child = (*line_box)[i];
    if (child.IsPlaceholder())
      continue;
    if (unsigned box_data_index = child.box_data_index) {
      BoxData& box_data = box_data_list_[box_data_index - 1];
      if (!box_data.fragment_end)
        box_data.fragment_start = i;
      box_data.fragment_end = i + 1;
    }
  }

  // Extend start/end of boxes when they are nested.
  for (BoxData& box_data : box_data_list_) {
    if (box_data.box_data_index) {
      BoxData& parent_box_data = box_data_list_[box_data.box_data_index - 1];
      if (!parent_box_data.fragment_end) {
        parent_box_data.fragment_start = box_data.fragment_start;
        parent_box_data.fragment_end = box_data.fragment_end;
      } else {
        parent_box_data.fragment_start =
            std::min(box_data.fragment_start, parent_box_data.fragment_start);
        parent_box_data.fragment_end =
            std::max(box_data.fragment_end, parent_box_data.fragment_end);
      }
    }
  }

#if DCHECK_IS_ON()
  // Check all BoxData have ranges.
  for (const BoxData& box_data : box_data_list_) {
    DCHECK_NE(box_data.fragment_end, 0u);
    DCHECK_GT(box_data.fragment_end, box_data.fragment_start);
  }
#endif
}

LayoutUnit NGInlineLayoutStateStack::ComputeInlinePositions(
    NGLineBoxFragmentBuilder::ChildList* line_box) {
  // At this point, children are in the visual order, and they have their
  // origins at (0, 0). Accumulate inline offset from left to right.
  LayoutUnit position;
  for (NGLineBoxFragmentBuilder::Child& child : *line_box) {
    child.offset.inline_offset += position;
    // Box margins/boders/paddings will be processed later.
    // TODO(kojii): we could optimize this if the reordering did not occur.
    if (!child.HasFragment())
      continue;
    position += child.inline_size;
  }

  if (box_data_list_.IsEmpty())
    return position;

  // Adjust child offsets for margin/border/padding of inline boxes.
  for (BoxData& box_data : box_data_list_) {
    unsigned start = box_data.fragment_start;
    unsigned end = box_data.fragment_end;
    DCHECK_GT(end, start);

    if (box_data.margin_border_padding_line_left) {
      line_box->MoveInInlineDirection(box_data.margin_border_padding_line_left,
                                      start, line_box->size());
      position += box_data.margin_border_padding_line_left;
    }

    if (box_data.margin_border_padding_line_right) {
      line_box->MoveInInlineDirection(box_data.margin_border_padding_line_right,
                                      end, line_box->size());
      position += box_data.margin_border_padding_line_right;
    }
  }

  // Compute positions and sizes of inline boxes.
  //
  // Accumulate margin/border/padding of boxes for each child, to place nested
  // parent boxes relative to the leaf (text or atomic inline) child.
  struct LinePadding {
    LayoutUnit line_left;
    LayoutUnit line_right;
  };
  Vector<LinePadding, 32> accumulated_padding(line_box->size());
  for (BoxData& box_data : box_data_list_) {
    // Compute line-left and line-right edge of this box by accomodating
    // border/padding of this box and margin/border/padding of descendants
    // boxes, while accumulating its margin/border/padding.
    unsigned start = box_data.fragment_start;
    NGLineBoxFragmentBuilder::Child& start_child = (*line_box)[start];
    LayoutUnit line_left_offset = start_child.offset.inline_offset;
    LinePadding& start_padding = accumulated_padding[start];
    start_padding.line_left += box_data.margin_border_padding_line_left;
    line_left_offset -= start_padding.line_left - box_data.margin_line_left;

    DCHECK_GT(box_data.fragment_end, start);
    unsigned last = box_data.fragment_end - 1;
    NGLineBoxFragmentBuilder::Child& last_child = (*line_box)[last];
    LayoutUnit line_right_offset =
        last_child.offset.inline_offset + last_child.inline_size;
    LinePadding& last_padding = accumulated_padding[last];
    last_padding.line_right += box_data.margin_border_padding_line_right;
    line_right_offset += last_padding.line_right - box_data.margin_line_right;

    box_data.offset.inline_offset = line_left_offset;
    box_data.size.inline_size = line_right_offset - line_left_offset;
  }

  return position;
}

void NGInlineLayoutStateStack::CreateBoxFragments(
    NGLineBoxFragmentBuilder::ChildList* line_box) {
  DCHECK(!box_data_list_.IsEmpty());

  for (BoxData& box_data : box_data_list_) {
    unsigned start = box_data.fragment_start;
    unsigned end = box_data.fragment_end;
    DCHECK_GT(end, start);
    NGLineBoxFragmentBuilder::Child& start_child = (*line_box)[start];

    scoped_refptr<NGLayoutResult> box_fragment =
        box_data.CreateBoxFragment(line_box);
    if (!start_child.HasFragment()) {
      start_child.layout_result = std::move(box_fragment);
      start_child.offset = box_data.offset;
    } else {
      // In most cases, |start_child| is moved to the children of the box, and
      // is empty. It's not empty when it's out-of-flow. Insert in such case.
      line_box->InsertChild(start, std::move(box_fragment), box_data.offset,
                            LayoutUnit(), 0);
    }
  }

  box_data_list_.clear();
}

scoped_refptr<NGLayoutResult>
NGInlineLayoutStateStack::BoxData::CreateBoxFragment(
    NGLineBoxFragmentBuilder::ChildList* line_box) {
  DCHECK(item);
  DCHECK(item->Style());
  const ComputedStyle& style = *item->Style();
  // Because children are already in the visual order, use LTR for the
  // fragment builder so that it should not transform the coordinates for RTL.
  NGBoxFragmentBuilder box(item->GetLayoutObject(), &style,
                           style.GetWritingMode(), TextDirection::kLtr);
  box.SetBoxType(NGPhysicalFragment::kInlineBox);
  box.SetStyleVariant(item->StyleVariant());

  // Inline boxes have block start/end borders, even when its containing block
  // was fragmented. Fragmenting a line box in block direction is not
  // supported today.
  box.SetBorderEdges({true, has_line_right_edge, true, has_line_left_edge});
  box.SetInlineSize(size.inline_size.ClampNegativeToZero());
  box.SetBlockSize(size.block_size);
  box.SetPadding(padding);

  for (unsigned i = fragment_start; i < fragment_end; i++) {
    NGLineBoxFragmentBuilder::Child& child = (*line_box)[i];
    if (child.layout_result) {
      box.AddChild(*child.layout_result, child.offset - offset);
      child.layout_result.reset();
    } else if (child.fragment) {
      box.AddChild(std::move(child.fragment), child.offset - offset);
    }
    // Leave out-of-flow fragments. They need to be at the top level so that
    // NGInlineLayoutAlgorithm can handle them later.
    DCHECK(!child.HasInFlowFragment());
  }
  box.MoveOutOfFlowDescendantCandidatesToDescendants(inline_container);
  return box.ToInlineBoxFragment();
}

NGInlineLayoutStateStack::PositionPending
NGInlineLayoutStateStack::ApplyBaselineShift(
    NGInlineBoxState* box,
    NGLineBoxFragmentBuilder::ChildList* line_box,
    FontBaseline baseline_type) {
  // Some 'vertical-align' values require the size of their parents. Align all
  // such descendant boxes that require the size of this box; they are queued in
  // |pending_descendants|.
  LayoutUnit baseline_shift;
  if (!box->pending_descendants.IsEmpty()) {
    NGLineHeightMetrics max = box->MetricsForTopAndBottomAlign();
    for (NGPendingPositions& child : box->pending_descendants) {
      // In quirks mode, metrics is empty if no content.
      if (child.metrics.IsEmpty())
        child.metrics = NGLineHeightMetrics::Zero();
      switch (child.vertical_align) {
        case EVerticalAlign::kTextTop:
          DCHECK(!box->text_metrics.IsEmpty());
          baseline_shift = child.metrics.ascent + box->text_top;
          break;
        case EVerticalAlign::kTop:
          baseline_shift = child.metrics.ascent - max.ascent;
          break;
        case EVerticalAlign::kTextBottom:
          if (const SimpleFontData* font_data =
                  box->style->GetFont().PrimaryFont()) {
            LayoutUnit text_bottom =
                font_data->GetFontMetrics().FixedDescent(baseline_type);
            baseline_shift = text_bottom - child.metrics.descent;
            break;
          }
          NOTREACHED();
          FALLTHROUGH;
        case EVerticalAlign::kBottom:
          baseline_shift = max.descent - child.metrics.descent;
          break;
        default:
          NOTREACHED();
          continue;
      }
      child.metrics.Move(baseline_shift);
      box->metrics.Unite(child.metrics);
      line_box->MoveInBlockDirection(baseline_shift, child.fragment_start,
                                     child.fragment_end);
    }
    box->pending_descendants.clear();
  }

  const ComputedStyle& style = *box->style;
  EVerticalAlign vertical_align = style.VerticalAlign();
  if (vertical_align == EVerticalAlign::kBaseline)
    return kPositionNotPending;

  // 'vertical-align' aligns boxes relative to themselves, to their parent
  // boxes, or to the line box, depends on the value.
  // Because |box| is an item in |stack_|, |box[-1]| is its parent box.
  // If this box doesn't have a parent; i.e., this box is a line box,
  // 'vertical-align' has no effect.
  DCHECK(box >= stack_.begin() && box < stack_.end());
  if (box == stack_.begin())
    return kPositionNotPending;
  NGInlineBoxState& parent_box = box[-1];

  // Check if there are any fragments to move.
  unsigned fragment_end = line_box->size();
  if (box->fragment_start == fragment_end)
    return kPositionNotPending;

  switch (vertical_align) {
    case EVerticalAlign::kSub:
      baseline_shift = parent_box.style->ComputedFontSizeAsFixed() / 5 + 1;
      break;
    case EVerticalAlign::kSuper:
      baseline_shift = -(parent_box.style->ComputedFontSizeAsFixed() / 3 + 1);
      break;
    case EVerticalAlign::kLength: {
      // 'Percentages: refer to the 'line-height' of the element itself'.
      // https://www.w3.org/TR/CSS22/visudet.html#propdef-vertical-align
      const Length& length = style.GetVerticalAlignLength();
      LayoutUnit line_height = length.IsPercentOrCalc()
                                   ? style.ComputedLineHeightAsFixed()
                                   : box->text_metrics.LineHeight();
      baseline_shift = -ValueForLength(length, line_height);
      break;
    }
    case EVerticalAlign::kMiddle:
      baseline_shift = (box->metrics.ascent - box->metrics.descent) / 2;
      if (const SimpleFontData* parent_font_data =
              parent_box.style->GetFont().PrimaryFont()) {
        baseline_shift -= LayoutUnit::FromFloatRound(
            parent_font_data->GetFontMetrics().XHeight() / 2);
      }
      break;
    case EVerticalAlign::kBaselineMiddle:
      baseline_shift = (box->metrics.ascent - box->metrics.descent) / 2;
      break;
    case EVerticalAlign::kTop:
    case EVerticalAlign::kBottom: {
      // 'top' and 'bottom' require the layout size of the nearest ancestor that
      // has 'top' or 'bottom', or the line box if none.
      NGInlineBoxState* ancestor = &parent_box;
      for (; ancestor != stack_.begin(); --ancestor) {
        if (ancestor->style->VerticalAlign() == EVerticalAlign::kTop ||
            ancestor->style->VerticalAlign() == EVerticalAlign::kBottom)
          break;
      }
      ancestor->pending_descendants.push_back(NGPendingPositions{
          box->fragment_start, fragment_end, box->metrics, vertical_align});
      return kPositionPending;
    }
    default:
      // Other values require the layout size of the parent box.
      parent_box.pending_descendants.push_back(NGPendingPositions{
          box->fragment_start, fragment_end, box->metrics, vertical_align});
      return kPositionPending;
  }
  if (!box->metrics.IsEmpty())
    box->metrics.Move(baseline_shift);
  line_box->MoveInBlockDirection(baseline_shift, box->fragment_start,
                                 fragment_end);
  return kPositionNotPending;
}

NGLineHeightMetrics NGInlineBoxState::MetricsForTopAndBottomAlign() const {
  // |metrics| is the bounds of "aligned subtree", that is, bounds of
  // descendants that are not 'vertical-align: top' nor 'bottom'.
  // https://drafts.csswg.org/css2/visudet.html#propdef-vertical-align
  NGLineHeightMetrics box = metrics;

  // In quirks mode, metrics is empty if no content.
  if (box.IsEmpty())
    box = NGLineHeightMetrics::Zero();

  // If the height of a box that has 'vertical-align: top' or 'bottom' exceeds
  // the height of the "aligned subtree", align the edge to the "aligned
  // subtree" and extend the other edge.
  NGLineHeightMetrics max = box;
  for (const NGPendingPositions& child : pending_descendants) {
    if ((child.vertical_align == EVerticalAlign::kTop ||
         child.vertical_align == EVerticalAlign::kBottom) &&
        child.metrics.LineHeight() > max.LineHeight()) {
      if (child.vertical_align == EVerticalAlign::kTop) {
        max = NGLineHeightMetrics(box.ascent,
                                  child.metrics.LineHeight() - box.ascent);
      } else if (child.vertical_align == EVerticalAlign::kBottom) {
        max = NGLineHeightMetrics(child.metrics.LineHeight() - box.descent,
                                  box.descent);
      }
    }
  }
  return max;
}

#if DCHECK_IS_ON()
void NGInlineLayoutStateStack::CheckSame(
    const NGInlineLayoutStateStack& other) const {
  // At the beginning of each line, box_data_list_ should be empty.
  DCHECK_EQ(box_data_list_.size(), 0u);
  DCHECK_EQ(other.box_data_list_.size(), 0u);

  DCHECK_EQ(stack_.size(), other.stack_.size());
  for (unsigned i = 0; i < stack_.size(); i++) {
    stack_[i].CheckSame(other.stack_[i]);
  }
}

void NGInlineBoxState::CheckSame(const NGInlineBoxState& other) const {
  DCHECK_EQ(fragment_start, other.fragment_start);
  DCHECK_EQ(item, other.item);
  DCHECK_EQ(style, other.style);
  DCHECK_EQ(inline_container, other.inline_container);

  DCHECK_EQ(metrics, other.metrics);
  DCHECK_EQ(text_metrics, other.text_metrics);
  DCHECK_EQ(text_top, other.text_top);
  DCHECK_EQ(text_height, other.text_height);
  if (!text_metrics.IsEmpty()) {
    // |include_used_fonts| will be computed when computing |text_metrics|.
    DCHECK_EQ(include_used_fonts, other.include_used_fonts);
  }

  DCHECK_EQ(needs_box_fragment, other.needs_box_fragment);

  DCHECK_EQ(has_start_edge, other.has_start_edge);
  // |has_end_edge| may not match because it will be computed in |OnCloseTag|.

  DCHECK_EQ(margin_inline_start, other.margin_inline_start);
  DCHECK_EQ(margin_inline_end, other.margin_inline_end);
  DCHECK_EQ(borders, other.borders);
  DCHECK_EQ(padding, other.padding);

  // At the beginning of each line, box_data_list_pending_descendants should be
  // empty.
  DCHECK_EQ(pending_descendants.size(), 0u);
  DCHECK_EQ(other.pending_descendants.size(), 0u);
}
#endif

}  // namespace blink
