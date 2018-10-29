// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_breaker.h"

#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shaping_line_breaker.h"

namespace blink {

namespace {

// CSS-defined white space characters, excluding the newline character.
// In most cases, the line breaker consider break opportunities are before
// spaces because it handles trailing spaces differently from other normal
// characters, but breaking before newline characters is not desired.
inline bool IsBreakableSpace(UChar c) {
  return c == kSpaceCharacter || c == kTabulationCharacter;
}

inline bool CanBreakAfterLast(const NGInlineItemResults& item_results) {
  return !item_results.IsEmpty() && item_results.back().can_break_after;
}

inline bool ShouldCreateLineBox(const NGInlineItemResults& item_results) {
  return !item_results.IsEmpty() && item_results.back().should_create_line_box;
}

}  // namespace

NGLineBreaker::NGLineBreaker(NGInlineNode node,
                             NGLineBreakerMode mode,
                             const NGConstraintSpace& space,
                             Vector<NGPositionedFloat>* positioned_floats,
                             NGUnpositionedFloatVector* unpositioned_floats,
                             NGContainerFragmentBuilder* container_builder,
                             NGExclusionSpace* exclusion_space,
                             unsigned handled_float_index,
                             const NGLineLayoutOpportunity& line_opportunity,
                             const NGInlineBreakToken* break_token)
    : line_opportunity_(line_opportunity),
      node_(node),
      is_first_formatted_line_((!break_token || (!break_token->ItemIndex() &&
                                                 !break_token->TextOffset())) &&
                               node.CanContainFirstFormattedLine()),
      use_first_line_style_(is_first_formatted_line_ &&
                            node.GetLayoutBox()
                                ->GetDocument()
                                .GetStyleEngine()
                                .UsesFirstLineRules()),
      in_line_height_quirks_mode_(node.InLineHeightQuirksMode()),
      items_data_(node.ItemsData(use_first_line_style_)),
      mode_(mode),
      constraint_space_(space),
      positioned_floats_(positioned_floats),
      unpositioned_floats_(unpositioned_floats),
      container_builder_(container_builder),
      exclusion_space_(exclusion_space),
      break_iterator_(items_data_.text_content),
      shaper_(items_data_.text_content),
      spacing_(items_data_.text_content),
      handled_floats_end_item_index_(handled_float_index),
      base_direction_(node_.BaseDirection()) {
  break_iterator_.SetBreakSpace(BreakSpaceType::kBeforeSpaceRun);

  if (break_token) {
    current_style_ = break_token->Style();
    item_index_ = break_token->ItemIndex();
    offset_ = break_token->TextOffset();
    break_iterator_.SetStartOffset(offset_);
    is_after_forced_break_ = break_token->IsForcedBreak();
    items_data_.AssertOffset(item_index_, offset_);
    ignore_floats_ = break_token->IgnoreFloats();
  }
}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGLineBreaker::~NGLineBreaker() = default;

inline NGInlineItemResult* NGLineBreaker::AddItem(const NGInlineItem& item,
                                                  unsigned end_offset) {
  DCHECK_LE(end_offset, item.EndOffset());
  return &item_results_->emplace_back(&item, item_index_, offset_, end_offset,
                                      ShouldCreateLineBox(*item_results_));
}

inline NGInlineItemResult* NGLineBreaker::AddItem(const NGInlineItem& item) {
  return AddItem(item, item.EndOffset());
}

void NGLineBreaker::SetLineEndFragment(
    scoped_refptr<const NGPhysicalTextFragment> fragment) {
  bool is_horizontal =
      IsHorizontalWritingMode(constraint_space_.GetWritingMode());
  if (line_info_->LineEndFragment()) {
    const NGPhysicalSize& size = line_info_->LineEndFragment()->Size();
    position_ -= is_horizontal ? size.width : size.height;
  }
  if (fragment) {
    const NGPhysicalSize& size = fragment->Size();
    position_ += is_horizontal ? size.width : size.height;
  }
  line_info_->SetLineEndFragment(std::move(fragment));
}

inline void NGLineBreaker::ComputeCanBreakAfter(
    NGInlineItemResult* item_result) const {
  item_result->can_break_after =
      auto_wrap_ && break_iterator_.IsBreakable(item_result->end_offset);
}

// Compute the base direction for bidi algorithm for this line.
void NGLineBreaker::ComputeBaseDirection() {
  // If 'unicode-bidi' is not 'plaintext', use the base direction of the block.
  if (!previous_line_had_forced_break_ ||
      node_.Style().GetUnicodeBidi() != UnicodeBidi::kPlaintext)
    return;
  // If 'unicode-bidi: plaintext', compute the base direction for each paragraph
  // (separated by forced break.)
  const String& text = Text();
  if (text.Is8Bit())
    return;
  wtf_size_t end_offset = text.find(kNewlineCharacter, offset_);
  base_direction_ = NGBidiParagraph::BaseDirectionForString(
      end_offset == kNotFound
          ? StringView(text, offset_)
          : StringView(text, offset_, end_offset - offset_));
}

// Initialize internal states for the next line.
void NGLineBreaker::PrepareNextLine() {
  // NGLineInfo is not supposed to be re-used becase it's not much gain and to
  // avoid rare code path.
  DCHECK(item_results_->IsEmpty());

  if (item_index_) {
    // We're past the first line
    previous_line_had_forced_break_ = is_after_forced_break_;
    is_after_forced_break_ = false;
    is_first_formatted_line_ = false;
    use_first_line_style_ = false;
  }

  line_info_->SetStartOffset(offset_);
  line_info_->SetLineStyle(node_, items_data_, constraint_space_,
                           is_first_formatted_line_, use_first_line_style_,
                           previous_line_had_forced_break_);
  // Set the initial style of this line from the break token. Example:
  //   <p>...<span>....</span></p>
  // When the line wraps in <span>, the 2nd line needs to start with the style
  // of the <span>.
  SetCurrentStyle(current_style_ ? *current_style_ : line_info_->LineStyle());
  ComputeBaseDirection();
  line_info_->SetBaseDirection(base_direction_);

  // Use 'text-indent' as the initial position. This lets tab positions to align
  // regardless of 'text-indent'.
  position_ = line_info_->TextIndent();
}

void NGLineBreaker::NextLine(NGLineInfo* line_info) {
  line_info_ = line_info;
  item_results_ = line_info->MutableResults();

  PrepareNextLine();
  BreakLine();
  if (!trailing_spaces_collapsed_)
    RemoveTrailingCollapsibleSpace();

#if DCHECK_IS_ON()
  for (const auto& result : *item_results_)
    result.CheckConsistency();
#endif

  // We should create a line-box when:
  //  - We have an item which needs a line box (text, etc).
  //  - A list-marker is present, and it would be the last line or last line
  //    before a forced new-line.
  //  - During min/max content sizing (to correctly determine the line width).
  //
  // TODO(kojii): There are cases where we need to PlaceItems() without creating
  // line boxes. These cases need to be reviewed.
  bool should_create_line_box =
      ShouldCreateLineBox(*item_results_) ||
      (has_list_marker_ && line_info_->IsLastLine()) ||
      mode_ != NGLineBreakerMode::kContent;

  if (!should_create_line_box)
    line_info_->SetIsEmptyLine();
  line_info_->SetEndItemIndex(item_index_);

  ComputeLineLocation();

  line_info_ = nullptr;
  item_results_ = nullptr;
}

void NGLineBreaker::BreakLine() {
  const Vector<NGInlineItem>& items = Items();
  state_ = LineBreakState::kLeading;
  while (state_ != LineBreakState::kDone) {
    // Check overflow even if |item_index_| is at the end of the block, because
    // the last item of the block may have caused overflow. In that case,
    // |HandleOverflow| will rewind |item_index_|.
    if (state_ == LineBreakState::kContinue && auto_wrap_ &&
        position_ > AvailableWidthToFit()) {
      HandleOverflow();
    }

    // If we reach at the end of the block, this is the last line.
    DCHECK_LE(item_index_, items.size());
    if (item_index_ == items.size()) {
      line_info_->SetIsLastLine(true);
      return;
    }

    // Handle trailable items first. These items may not be break before.
    // They (or part of them) may also overhang the available width.
    const NGInlineItem& item = items[item_index_];
    if (item.Type() == NGInlineItem::kText) {
      HandleText(item);
#if DCHECK_IS_ON()
      if (!item_results_->IsEmpty())
        item_results_->back().CheckConsistency(true);
#endif
      continue;
    }
    if (item.Type() == NGInlineItem::kCloseTag) {
      HandleCloseTag(item);
      continue;
    }
    if (item.Type() == NGInlineItem::kControl) {
      HandleControlItem(item);
      continue;
    }
    if (item.Type() == NGInlineItem::kFloating) {
      HandleFloat(item);
      continue;
    }
    if (item.Type() == NGInlineItem::kBidiControl) {
      HandleBidiControlItem(item);
      continue;
    }

    // Items after this point are not trailable. Break at the earliest break
    // opportunity if we're trailing.
    if (state_ == LineBreakState::kTrailing &&
        CanBreakAfterLast(*item_results_)) {
      line_info_->SetIsLastLine(false);
      return;
    }

    if (item.Type() == NGInlineItem::kAtomicInline) {
      HandleAtomicInline(item);
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      HandleOpenTag(item);
    } else if (item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      AddItem(item);
      MoveToNextOf(item);
    } else if (item.Length()) {
      NOTREACHED();
      // For other items with text (e.g., bidi controls), use their text to
      // determine the break opportunity.
      NGInlineItemResult* item_result = AddItem(item);
      item_result->can_break_after =
          break_iterator_.IsBreakable(item_result->end_offset);
      MoveToNextOf(item);
    } else if (item.Type() == NGInlineItem::kListMarker) {
      NGInlineItemResult* item_result = AddItem(item);
      has_list_marker_ = true;
      DCHECK(!item_result->can_break_after);
      MoveToNextOf(item);
    } else {
      NOTREACHED();
      MoveToNextOf(item);
    }
  }
}

// Re-compute the current position from NGLineInfo.
// The current position is usually updated as NGLineBreaker builds
// NGInlineItemResults. This function re-computes it when it was lost.
void NGLineBreaker::UpdatePosition() {
  position_ = line_info_->ComputeWidth();
}

void NGLineBreaker::ComputeLineLocation() const {
  // Negative margins can make the position negative, but the inline size is
  // always positive or 0.
  LayoutUnit available_width = AvailableWidth();
  DCHECK_EQ(position_, line_info_->ComputeWidth());

  line_info_->SetWidth(available_width, position_);
  line_info_->SetBfcOffset(
      {line_opportunity_.line_left_offset, line_opportunity_.bfc_block_offset});
}

void NGLineBreaker::HandleText(const NGInlineItem& item) {
  DCHECK_EQ(item.Type(), NGInlineItem::kText);
  DCHECK(item.TextShapeResult());

  // If we're trailing, only trailing spaces can be included in this line.
  if (state_ == LineBreakState::kTrailing &&
      CanBreakAfterLast(*item_results_)) {
    return HandleTrailingSpaces(item);
  }

  // Skip leading collapsible spaces.
  // Most cases such spaces are handled as trailing spaces of the previous line,
  // but there are some cases doing so is too complex.
  if (state_ == LineBreakState::kLeading) {
    state_ = LineBreakState::kContinue;
    if (item.Style()->CollapseWhiteSpace() &&
        Text()[offset_] == kSpaceCharacter) {
      // Skipping one whitespace removes all collapsible spaces because
      // collapsible spaces are collapsed to single space in
      // NGInlineItemBuilder.
      ++offset_;
      if (offset_ == item.EndOffset()) {
        MoveToNextOf(item);
        return;
      }
    }
  }

  NGInlineItemResult* item_result = AddItem(item);
  item_result->should_create_line_box = true;
  LayoutUnit available_width = AvailableWidthToFit();

  if (auto_wrap_) {
    // Try to break inside of this text item.
    BreakText(item_result, item, available_width - position_);

    if (item.IsSymbolMarker()) {
      LayoutUnit symbol_width = LayoutListMarker::WidthOfSymbol(*item.Style());
      if (symbol_width > 0)
        item_result->inline_size = symbol_width;
    }

    LayoutUnit next_position = position_ + item_result->inline_size;
    bool is_overflow = next_position > available_width;
    DCHECK(is_overflow || item_result->shape_result);
    position_ = next_position;
    item_result->may_break_inside = !is_overflow;
    MoveToNextOf(*item_result);

    if (!is_overflow ||
        (state_ == LineBreakState::kTrailing && item_result->shape_result)) {
      if (item_result->end_offset < item.EndOffset()) {
        // The break point found, and text follows. Break here, after trailing
        // spaces.
        return HandleTrailingSpaces(item);
      }

      // The break point found, but items that prohibit breaking before them may
      // follow. Continue looking next items.
      return;
    }

    return HandleOverflow();
  }

  // Add the rest of the item if !auto_wrap.
  // Because the start position may need to reshape, run ShapingLineBreaker
  // with max available width.
  BreakText(item_result, item, LayoutUnit::Max());

  if (item.IsSymbolMarker()) {
    LayoutUnit symbol_width = LayoutListMarker::WidthOfSymbol(*item.Style());
    if (symbol_width > 0)
      item_result->inline_size = symbol_width;
  }

  DCHECK_EQ(item_result->end_offset, item.EndOffset());
  DCHECK(!item_result->may_break_inside);
  item_result->can_break_after = false;
  position_ += item_result->inline_size;
  MoveToNextOf(item);
}

void NGLineBreaker::BreakText(NGInlineItemResult* item_result,
                              const NGInlineItem& item,
                              LayoutUnit available_width) {
  DCHECK_EQ(item.Type(), NGInlineItem::kText);
  item.AssertOffset(item_result->start_offset);

  // TODO(kojii): We need to instantiate ShapingLineBreaker here because it
  // has item-specific info as context. Should they be part of ShapeLine() to
  // instantiate once, or is this just fine since instatiation is not
  // expensive?
  DCHECK_EQ(item.TextShapeResult()->StartIndexForResult(), item.StartOffset());
  DCHECK_EQ(item.TextShapeResult()->EndIndexForResult(), item.EndOffset());
  RunSegmenter::RunSegmenterRange segment_range =
      item.CreateRunSegmenterRange();
  ShapingLineBreaker breaker(&shaper_, &item.Style()->GetFont(),
                             item.TextShapeResult(), &break_iterator_,
                             &segment_range, &spacing_, hyphenation_);
  if (!enable_soft_hyphen_)
    breaker.DisableSoftHyphen();
  available_width = std::max(LayoutUnit(0), available_width);

  // Use kStartShouldBeSafe if at the beginning of a line.
  unsigned options = ShapingLineBreaker::kDefaultOptions;
  if (item_result->start_offset != line_info_->StartOffset())
    options |= ShapingLineBreaker::kDontReshapeStart;

  // Use kNoResultIfOverflow if 'break-word' and we're trying to break normally
  // because if this item overflows, we will rewind and break line again. The
  // overflowing ShapeResult is not needed.
  if (break_anywhere_if_overflow_ && !override_break_anywhere_)
    options |= ShapingLineBreaker::kNoResultIfOverflow;
  ShapingLineBreaker::Result result;
  scoped_refptr<const ShapeResult> shape_result = breaker.ShapeLine(
      item_result->start_offset, available_width, options, &result);

  // If this item overflows and 'break-word' is set, this line will be
  // rewinded. Making this item long enough to overflow is enough.
  if (!shape_result) {
    DCHECK(options & ShapingLineBreaker::kNoResultIfOverflow);
    item_result->inline_size = available_width + 1;
    item_result->end_offset = item.EndOffset();
    return;
  }
  DCHECK_EQ(shape_result->NumCharacters(),
            result.break_offset - item_result->start_offset);

  if (result.is_hyphenated) {
    AppendHyphen(item);
    // TODO(kojii): Implement when adding a hyphen caused overflow.
    // crbug.com/714962: Should be removed when switched to NGPaint.
    item_result->text_end_effect = NGTextEndEffect::kHyphen;
  }
  item_result->inline_size = shape_result->SnappedWidth().ClampNegativeToZero();
  item_result->end_offset = result.break_offset;
  item_result->shape_result = std::move(shape_result);
  DCHECK_GT(item_result->end_offset, item_result->start_offset);

  // * If width <= available_width:
  //   * If offset < item.EndOffset(): the break opportunity to fit is found.
  //   * If offset == item.EndOffset(): the break opportunity at the end fits,
  //     or the first break opportunity is beyond the end.
  //     There may be room for more characters.
  // * If width > available_width: The first break opportunity does not fit.
  //   offset is the first break opportunity, either inside, at the end, or
  //   beyond the end.
  if (item_result->end_offset < item.EndOffset()) {
    item_result->can_break_after = true;
  } else {
    DCHECK_EQ(item_result->end_offset, item.EndOffset());
    item_result->can_break_after =
        break_iterator_.IsBreakable(item_result->end_offset);
  }
}

// Re-shape the specified range of |NGInlineItem|.
scoped_refptr<ShapeResult> NGLineBreaker::ShapeText(const NGInlineItem& item,
                                                    unsigned start,
                                                    unsigned end) {
  RunSegmenter::RunSegmenterRange segment_range =
      item.CreateRunSegmenterRange();
  scoped_refptr<ShapeResult> result = shaper_.Shape(
      &item.Style()->GetFont(), item.TextShapeResult()->Direction(), start, end,
      &segment_range);
  if (UNLIKELY(spacing_.HasSpacing()))
    result->ApplySpacing(spacing_);
  return result;
}

// Compute a new ShapeResult for the specified end offset.
// The end is re-shaped if it is not safe-to-break.
scoped_refptr<ShapeResult> NGLineBreaker::TruncateLineEndResult(
    const NGInlineItemResult& item_result,
    unsigned end_offset) {
  DCHECK(item_result.item);
  DCHECK(item_result.shape_result);
  const ShapeResult& source_result = *item_result.shape_result;
  const unsigned start_offset = item_result.start_offset;
  DCHECK_GE(start_offset, source_result.StartIndexForResult());
  DCHECK_LE(end_offset, source_result.EndIndexForResult());
  DCHECK(start_offset > source_result.StartIndexForResult() ||
         end_offset < source_result.EndIndexForResult());

  scoped_refptr<ShapeResult> new_result;
  unsigned last_safe = source_result.PreviousSafeToBreakOffset(end_offset);
  DCHECK_LE(last_safe, end_offset);
  if (last_safe > start_offset)
    new_result = source_result.SubRange(start_offset, last_safe);
  if (last_safe < end_offset) {
    scoped_refptr<ShapeResult> end_result = ShapeText(
        *item_result.item, std::max(last_safe, start_offset), end_offset);
    if (new_result)
      end_result->CopyRange(0, end_offset, new_result.get());
    else
      new_result = std::move(end_result);
  }
  DCHECK(new_result);
  return new_result;
}

// Update |ShapeResult| in |item_result| to match to its |start_offset| and
// |end_offset|. The end is re-shaped if it is not safe-to-break.
void NGLineBreaker::UpdateShapeResult(NGInlineItemResult* item_result) {
  DCHECK(item_result);
  item_result->shape_result =
      TruncateLineEndResult(*item_result, item_result->end_offset);
  DCHECK(item_result->shape_result);
  item_result->inline_size = item_result->shape_result->SnappedWidth();
}

void NGLineBreaker::HandleTrailingSpaces(const NGInlineItem& item) {
  DCHECK_EQ(item.Type(), NGInlineItem::kText);
  DCHECK_LT(offset_, item.EndOffset());
  const String& text = Text();
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  if (style.CollapseWhiteSpace()) {
    if (text[offset_] != kSpaceCharacter) {
      state_ = LineBreakState::kDone;
      return;
    }

    // Skipping one whitespace removes all collapsible spaces because
    // collapsible spaces are collapsed to single space in NGInlineItemBuilder.
    offset_++;
    trailing_spaces_collapsed_ = true;

    // Make the last item breakable after, even if it was nowrap.
    DCHECK(!item_results_->IsEmpty());
    item_results_->back().can_break_after = true;
  } else {
    // Find the end of the run of space characters in this item.
    // Other white space characters (e.g., tab) are not included in this item.
    DCHECK(style.BreakOnlyAfterWhiteSpace());
    trailing_spaces_collapsed_ = true;
    unsigned end = offset_;
    while (end < item.EndOffset() && text[end] == kSpaceCharacter)
      end++;
    if (end == offset_) {
      state_ = LineBreakState::kDone;
      return;
    }

    NGInlineItemResult* item_result = AddItem(item, end);
    item_result->has_only_trailing_spaces = true;
    item_result->shape_result = item.TextShapeResult();
    if (item_result->start_offset == item.StartOffset() &&
        item_result->end_offset == item.EndOffset())
      item_result->inline_size = item_result->shape_result->SnappedWidth();
    else
      UpdateShapeResult(item_result);
    position_ += item_result->inline_size;
    item_result->can_break_after =
        end < text.length() && !IsBreakableSpace(text[end]);
    offset_ = end;
  }

  // If non-space characters follow, the line is done.
  // Otherwise keep checking next items for the break point.
  DCHECK_LE(offset_, item.EndOffset());
  if (offset_ < item.EndOffset()) {
    state_ = LineBreakState::kDone;
    return;
  }
  item_index_++;
  state_ = LineBreakState::kTrailing;
}

// Remove trailing collapsible spaces in |line_info|.
// https://drafts.csswg.org/css-text-3/#white-space-phase-2
void NGLineBreaker::RemoveTrailingCollapsibleSpace() {
  DCHECK(!trailing_spaces_collapsed_);

  ComputeTrailingCollapsibleSpace();
  trailing_spaces_collapsed_ = true;
  if (!trailing_collapsible_space_.has_value())
    return;

  // We have a trailing collapsible space. Remove it.
  NGInlineItemResult* item_result = trailing_collapsible_space_->item_result;
  position_ -= item_result->inline_size;
  if (scoped_refptr<const ShapeResult>& collapsed_shape_result =
          trailing_collapsible_space_->collapsed_shape_result) {
    DCHECK_GE(item_result->end_offset, item_result->start_offset + 2);
    --item_result->end_offset;
    item_result->shape_result = std::move(collapsed_shape_result);
    item_result->inline_size = item_result->shape_result->SnappedWidth();
    position_ += item_result->inline_size;
  } else {
    item_results_->erase(item_result);
  }
  trailing_collapsible_space_.reset();
}

// Compute the width of trailing spaces without removing it.
LayoutUnit NGLineBreaker::TrailingCollapsibleSpaceWidth() {
  if (trailing_spaces_collapsed_)
    return LayoutUnit();

  ComputeTrailingCollapsibleSpace();
  if (!trailing_collapsible_space_.has_value())
    return LayoutUnit();

  // Normally, the width of new_reuslt is smaller, but technically it can be
  // larger. In such case, it means the trailing spaces has negative width.
  NGInlineItemResult* item_result = trailing_collapsible_space_->item_result;
  if (scoped_refptr<const ShapeResult>& collapsed_shape_result =
          trailing_collapsible_space_->collapsed_shape_result) {
    return item_result->inline_size - collapsed_shape_result->SnappedWidth();
  }
  return item_result->inline_size;
}

// Find trailing collapsible space if exists. The result is cached to
// |trailing_collapsible_space_|.
void NGLineBreaker::ComputeTrailingCollapsibleSpace() {
  DCHECK(!trailing_spaces_collapsed_);

  for (auto it = item_results_->rbegin(); it != item_results_->rend(); ++it) {
    NGInlineItemResult& item_result = *it;
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item.EndCollapseType() == NGInlineItem::kOpaqueToCollapsing)
      continue;
    if (item.Type() != NGInlineItem::kText)
      break;
    DCHECK_GT(item_result.end_offset, 0u);
    DCHECK(item.Style());
    if (Text()[item_result.end_offset - 1] != kSpaceCharacter ||
        !item.Style()->CollapseWhiteSpace() ||
        // |shape_result| is nullptr if this is an overflow because BreakText()
        // uses kNoResultIfOverflow option.
        !item_result.shape_result)
      break;

    if (!trailing_collapsible_space_.has_value() ||
        trailing_collapsible_space_->item_result != &item_result) {
      trailing_collapsible_space_.emplace();
      trailing_collapsible_space_->item_result = &item_result;
      if (item_result.end_offset - 1 > item_result.start_offset) {
        trailing_collapsible_space_->collapsed_shape_result =
            TruncateLineEndResult(item_result, item_result.end_offset - 1);
      }
    }
    return;
  }

  trailing_collapsible_space_.reset();
}

void NGLineBreaker::AppendHyphen(const NGInlineItem& item) {
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  TextDirection direction = style.Direction();
  String hyphen_string = style.HyphenString();
  HarfBuzzShaper shaper(hyphen_string);
  scoped_refptr<ShapeResult> hyphen_result =
      shaper.Shape(&style.GetFont(), direction);
  NGTextFragmentBuilder builder(node_, constraint_space_.GetWritingMode());
  builder.SetText(item.GetLayoutObject(), hyphen_string, &style,
                  /* is_ellipsis_style */ false, std::move(hyphen_result));
  SetLineEndFragment(builder.ToTextFragment());
}

// Measure control items; new lines and tab, that are similar to text, affect
// layout, but do not need shaping/painting.
void NGLineBreaker::HandleControlItem(const NGInlineItem& item) {
  DCHECK_EQ(item.Length(), 1u);

  UChar character = Text()[item.StartOffset()];
  switch (character) {
    case kNewlineCharacter: {
      NGInlineItemResult* item_result = AddItem(item);
      item_result->should_create_line_box = true;
      item_result->has_only_trailing_spaces = true;
      is_after_forced_break_ = true;
      line_info_->SetIsLastLine(true);
      state_ = LineBreakState::kDone;
      break;
    }
    case kTabulationCharacter: {
      NGInlineItemResult* item_result = AddItem(item);
      item_result->should_create_line_box = true;
      DCHECK(item.Style());
      const ComputedStyle& style = *item.Style();
      const Font& font = style.GetFont();
      item_result->inline_size = font.TabWidth(style.GetTabSize(), position_);
      position_ += item_result->inline_size;
      item_result->has_only_trailing_spaces =
          state_ == LineBreakState::kTrailing;
      ComputeCanBreakAfter(item_result);
      break;
    }
    case kZeroWidthSpaceCharacter: {
      // <wbr> tag creates break opportunities regardless of auto_wrap.
      NGInlineItemResult* item_result = AddItem(item);
      item_result->should_create_line_box = true;
      item_result->can_break_after = true;
      break;
    }
    case kCarriageReturnCharacter:
    case kFormFeedCharacter:
      // Ignore carriage return and form feed.
      // https://drafts.csswg.org/css-text-3/#white-space-processing
      // https://github.com/w3c/csswg-drafts/issues/855
      break;
    default:
      NOTREACHED();
      break;
  }
  MoveToNextOf(item);
}

void NGLineBreaker::HandleBidiControlItem(const NGInlineItem& item) {
  DCHECK_EQ(item.Length(), 1u);

  // Bidi control characters have enter/exit semantics. Handle "enter"
  // characters simialr to open-tag, while "exit" (pop) characters similar to
  // close-tag.
  UChar character = Text()[item.StartOffset()];
  bool is_pop = character == kPopDirectionalIsolateCharacter ||
                character == kPopDirectionalFormattingCharacter;
  if (is_pop) {
    if (!item_results_->IsEmpty()) {
      NGInlineItemResult* item_result = AddItem(item);
      NGInlineItemResult* last = &(*item_results_)[item_results_->size() - 2];
      item_result->can_break_after = last->can_break_after;
      last->can_break_after = false;
    } else {
      AddItem(item);
    }
  } else {
    if (state_ == LineBreakState::kTrailing &&
        CanBreakAfterLast(*item_results_)) {
      line_info_->SetIsLastLine(false);
      MoveToNextOf(item);
      state_ = LineBreakState::kDone;
      return;
    }
    NGInlineItemResult* item_result = AddItem(item);
    DCHECK(!item_result->can_break_after);
  }
  MoveToNextOf(item);
}

void NGLineBreaker::HandleAtomicInline(const NGInlineItem& item) {
  DCHECK_EQ(item.Type(), NGInlineItem::kAtomicInline);

  NGInlineItemResult* item_result = AddItem(item);
  item_result->should_create_line_box = true;
  // When we're just computing min/max content sizes, we can skip the full
  // layout and just compute those sizes. On the other hand, for regular
  // layout we need to do the full layout and get the layout result.
  // Doing a full layout for min/max content can also have undesirable
  // side effects when that falls back to legacy layout.
  if (mode_ == NGLineBreakerMode::kContent) {
    item_result->layout_result =
        NGBlockNode(ToLayoutBox(item.GetLayoutObject()))
            .LayoutAtomicInline(constraint_space_,
                                line_info_->LineStyle().GetFontBaseline(),
                                line_info_->UseFirstLineStyle());
    DCHECK(item_result->layout_result->PhysicalFragment());

    item_result->inline_size =
        NGFragment(constraint_space_.GetWritingMode(),
                   *item_result->layout_result->PhysicalFragment())
            .InlineSize();
  } else {
    NGBlockNode block_node(ToLayoutBox(item.GetLayoutObject()));
    MinMaxSizeInput input;
    MinMaxSize sizes = ComputeMinAndMaxContentContribution(
        constraint_space_.GetWritingMode(), block_node, input,
        &constraint_space_);
    item_result->inline_size = mode_ == NGLineBreakerMode::kMinContent
                                   ? sizes.min_size
                                   : sizes.max_size;
  }

  // For the inline layout purpose, only inline-margins are needed, computed for
  // the line's writing-mode.
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  item_result->margins =
      ComputeLineMarginsForVisualContainer(constraint_space_, style);
  item_result->inline_size += item_result->margins.InlineSum();

  if (state_ == LineBreakState::kLeading)
    state_ = LineBreakState::kContinue;
  position_ += item_result->inline_size;
  ComputeCanBreakAfter(item_result);
  MoveToNextOf(item);
}

// Performs layout and positions a float.
//
// If there is a known available_width (e.g. something has resolved the
// container BFC block offset) it will attempt to position the float on the
// current line.
// Additionally updates the available_width for the line as the float has
// (probably) consumed space.
//
// If the float is too wide *or* we already have UnpositionedFloats we add it
// as an UnpositionedFloat. This should be positioned *immediately* after we
// are done with the current line.
// We have this check if there are already UnpositionedFloats as we aren't
// allowed to position a float "above" another float which has come before us
// in the document.
void NGLineBreaker::HandleFloat(const NGInlineItem& item) {
  // When rewind occurs, an item may be handled multiple times.
  // Since floats are put into a separate list, avoid handling same floats
  // twice.
  // Ideally rewind can take floats out of floats list, but the difference is
  // sutble compared to the complexity.
  //
  // Additionally, we need to skip floats if we're retrying a line after a
  // fragmentainer break. In that case the floats associated with this line will
  // already have been processed.
  NGInlineItemResult* item_result = AddItem(item);
  ComputeCanBreakAfter(item_result);
  MoveToNextOf(item);
  if (item_index_ <= handled_floats_end_item_index_ || ignore_floats_)
    return;

  NGBlockNode node(ToLayoutBox(item.GetLayoutObject()));

  const ComputedStyle& float_style = node.Style();

  // TODO(ikilpatrick): Add support for float break tokens inside an inline
  // layout context.
  NGUnpositionedFloat unpositioned_float(node, /* break_token */ nullptr);

  // If we are currently computing our min/max-content size simply append
  // to the unpositioned floats list and abort.
  if (mode_ != NGLineBreakerMode::kContent) {
    AddUnpositionedFloat(unpositioned_floats_, container_builder_,
                         std::move(unpositioned_float));
    return;
  }

  LayoutUnit inline_margin_size =
      ComputeMarginBoxInlineSizeForUnpositionedFloat(constraint_space_,
                                                     &unpositioned_float);

  LayoutUnit bfc_block_offset = line_opportunity_.bfc_block_offset;

  bool can_fit_float =
      position_ + inline_margin_size <=
      line_opportunity_.AvailableFloatInlineSize().AddEpsilon();
  if (!can_fit_float) {
    // Floats need to know the current line width to determine whether to put it
    // into the current line or to the next line. Trailing spaces will be
    // removed if this line breaks here because they should be collapsed across
    // floats, but they are still included in the current line position at this
    // point. Exclude it when computing whether this float can fit or not.
    can_fit_float =
        position_ + inline_margin_size - TrailingCollapsibleSpaceWidth() <=
        line_opportunity_.AvailableFloatInlineSize().AddEpsilon();
  }

  // The float should be positioned after the current line if:
  //  - It can't fit within the non-shape area. (Assuming the current position
  //    also is strictly within the non-shape area).
  //  - It will be moved down due to block-start edge alignment.
  //  - It will be moved down due to clearance.
  bool float_after_line =
      !can_fit_float ||
      exclusion_space_->LastFloatBlockStart() > bfc_block_offset ||
      exclusion_space_->ClearanceOffset(float_style.Clear()) > bfc_block_offset;

  // Check if we already have a pending float. That's because a float cannot be
  // higher than any block or floated box generated before.
  if (!unpositioned_floats_->IsEmpty() || float_after_line) {
    AddUnpositionedFloat(unpositioned_floats_, container_builder_,
                         std::move(unpositioned_float));
  } else {
    NGPositionedFloat positioned_float = PositionFloat(
        constraint_space_.AvailableSize(),
        constraint_space_.PercentageResolutionSize(),
        constraint_space_.ReplacedPercentageResolutionSize(),
        {constraint_space_.BfcOffset().line_offset, bfc_block_offset},
        constraint_space_.BfcOffset().block_offset, &unpositioned_float,
        constraint_space_, exclusion_space_);
    positioned_floats_->push_back(positioned_float);

    NGLayoutOpportunity opportunity = exclusion_space_->FindLayoutOpportunity(
        {constraint_space_.BfcOffset().line_offset, bfc_block_offset},
        constraint_space_.AvailableSize().inline_size, NGLogicalSize());

    DCHECK_EQ(bfc_block_offset, opportunity.rect.BlockStartOffset());

    line_opportunity_ = opportunity.ComputeLineLayoutOpportunity(
        constraint_space_, line_opportunity_.line_block_size, LayoutUnit());

    DCHECK_GE(AvailableWidth(), LayoutUnit());
  }
}

bool NGLineBreaker::ComputeOpenTagResult(
    const NGInlineItem& item,
    const NGConstraintSpace& constraint_space,
    NGInlineItemResult* item_result) {
  DCHECK_EQ(item.Type(), NGInlineItem::kOpenTag);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  item_result->has_edge = item.HasStartEdge();
  if (item.ShouldCreateBoxFragment() &&
      (style.HasBorder() || style.HasPadding() ||
       (style.HasMargin() && item_result->has_edge))) {
    item_result->borders = ComputeLineBorders(constraint_space, style);
    item_result->padding = ComputeLinePadding(constraint_space, style);
    if (item_result->has_edge) {
      item_result->margins = ComputeLineMarginsForSelf(constraint_space, style);
      item_result->inline_size = item_result->margins.inline_start +
                                 item_result->borders.inline_start +
                                 item_result->padding.inline_start;
      return true;
    }
  }
  return false;
}

void NGLineBreaker::HandleOpenTag(const NGInlineItem& item) {
  NGInlineItemResult* item_result = AddItem(item);
  DCHECK(!item_result->can_break_after);

  if (ComputeOpenTagResult(item, constraint_space_, item_result)) {
    position_ += item_result->inline_size;

    // While the spec defines "non-zero margins, padding, or borders" prevents
    // line boxes to be zero-height, tests indicate that only inline direction
    // of them do so. See should_create_line_box_.
    // Force to create a box, because such inline boxes affect line heights.
    if (!item_result->should_create_line_box &&
        (item_result->inline_size ||
         (item_result->margins.inline_start && !in_line_height_quirks_mode_)))
      item_result->should_create_line_box = true;
  }

  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  SetCurrentStyle(style);
  MoveToNextOf(item);
}

void NGLineBreaker::HandleCloseTag(const NGInlineItem& item) {
  NGInlineItemResult* item_result = AddItem(item);
  item_result->has_edge = item.HasEndEdge();
  if (item_result->has_edge) {
    DCHECK(item.Style());
    const ComputedStyle& style = *item.Style();
    NGBoxStrut margins = ComputeMarginsForSelf(constraint_space_, style);
    NGBoxStrut borders = ComputeBorders(constraint_space_, style);
    NGBoxStrut paddings = ComputePadding(constraint_space_, style);
    item_result->inline_size =
        margins.inline_end + borders.inline_end + paddings.inline_end;
    position_ += item_result->inline_size;

    if (!item_result->should_create_line_box &&
        (item_result->inline_size ||
         (margins.inline_end && !in_line_height_quirks_mode_)))
      item_result->should_create_line_box = true;
  }
  DCHECK(item.GetLayoutObject() && item.GetLayoutObject()->Parent());
  bool was_auto_wrap = auto_wrap_;
  SetCurrentStyle(item.GetLayoutObject()->Parent()->StyleRef());
  MoveToNextOf(item);

  // Prohibit break before a close tag by setting can_break_after to the
  // previous result.
  // TODO(kojii): There should be a result before close tag, but there are cases
  // that doesn't because of the way we handle trailing spaces. This needs to be
  // revisited.
  if (item_results_->size() >= 2) {
    NGInlineItemResult* last = &(*item_results_)[item_results_->size() - 2];
    if (was_auto_wrap == auto_wrap_) {
      item_result->can_break_after = last->can_break_after;
      last->can_break_after = false;
      return;
    }
    last->can_break_after = false;
    if (!was_auto_wrap) {
      DCHECK(auto_wrap_);
      // When auto-wrap starts after no-wrap, the boundary is not allowed to
      // wrap. However, when space characters follow the boundary, there should
      // be a break opportunity after the space. The break_iterator cannot
      // compute this because it considers break opportunities are before a run
      // of spaces.
      const String& text = Text();
      if (offset_ < text.length() && IsBreakableSpace(text[offset_])) {
        item_result->can_break_after = true;
        return;
      }
    }
  }
  ComputeCanBreakAfter(item_result);
}

// Handles when the last item overflows.
// At this point, item_results does not fit into the current line, and there
// are no break opportunities in item_results.back().
void NGLineBreaker::HandleOverflow() {
  LayoutUnit available_width = AvailableWidthToFit();
  LayoutUnit width_to_rewind = position_ - available_width;
  DCHECK_GT(width_to_rewind, 0);
  bool position_maybe_changed = false;

  // Keep track of the shortest break opportunity.
  unsigned break_before = 0;

  // Search for a break opportunity that can fit.
  for (unsigned i = item_results_->size(); i;) {
    NGInlineItemResult* item_result = &(*item_results_)[--i];

    // Try to break after this item.
    if (i < item_results_->size() - 1 && item_result->can_break_after) {
      if (width_to_rewind <= 0) {
        position_ = available_width + width_to_rewind;
        Rewind(i + 1);
        state_ = LineBreakState::kTrailing;
        return;
      }
      break_before = i + 1;
    }

    // Try to break inside of this item.
    LayoutUnit next_width_to_rewind =
        width_to_rewind - item_result->inline_size;
    DCHECK(item_result->item);
    const NGInlineItem& item = *item_result->item;
    if (item.Type() == NGInlineItem::kText && next_width_to_rewind < 0 &&
        (item_result->may_break_inside || override_break_anywhere_)) {
      // When the text fits but its right margin does not, the break point
      // must not be at the end.
      LayoutUnit item_available_width =
          std::min(-next_width_to_rewind, item_result->inline_size - 1);
      SetCurrentStyle(*item.Style());
      BreakText(item_result, item, item_available_width);
#if DCHECK_IS_ON()
      item_result->CheckConsistency(true);
#endif
      // If BreakText() changed this item small enough to fit, break here.
      if (item_result->inline_size <= item_available_width) {
        DCHECK(item_result->end_offset < item.EndOffset());
        DCHECK(item_result->can_break_after);
        DCHECK_LE(i + 1, item_results_->size());
        if (i + 1 == item_results_->size()) {
          // If this is the last item, adjust states to accomodate the change.
          position_ =
              available_width + next_width_to_rewind + item_result->inline_size;
          if (line_info_->LineEndFragment())
            SetLineEndFragment(nullptr);
          DCHECK_EQ(position_, line_info_->ComputeWidth());
          item_index_ = item_result->item_index;
          offset_ = item_result->end_offset;
          items_data_.AssertOffset(item_index_, offset_);
        } else {
          Rewind(i + 1);
        }
        state_ = LineBreakState::kTrailing;
        return;
      }
      position_maybe_changed = true;
    }

    width_to_rewind = next_width_to_rewind;
  }

  // Reaching here means that the rewind point was not found.

  if (break_anywhere_if_overflow_ && !override_break_anywhere_) {
    override_break_anywhere_ = true;
    break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
    if (!item_results_->IsEmpty())
      Rewind(0);
    state_ = LineBreakState::kLeading;
    return;
  }

  // Let this line overflow.
  // If there was a break opportunity, the overflow should stop there.
  if (break_before) {
    Rewind(break_before);
    state_ = LineBreakState::kTrailing;
    return;
  }

  if (position_maybe_changed) {
    UpdatePosition();
  }

  state_ = LineBreakState::kTrailing;
}

void NGLineBreaker::Rewind(unsigned new_end) {
  NGInlineItemResults& item_results = *item_results_;
  DCHECK_LT(new_end, item_results.size());

  // Avoid rewinding floats if possible. They will be added back anyway while
  // processing trailing items even when zero available width. Also this saves
  // most cases where our support for rewinding positioned floats is not great
  // yet (see below.)
  while (item_results[new_end].item->Type() == NGInlineItem::kFloating) {
    ++new_end;
    if (new_end == item_results.size()) {
      UpdatePosition();
      return;
    }
  }

  // Because floats are added to |positioned_floats_| or |unpositioned_floats_|,
  // rewinding them needs to remove from these lists too.
  for (unsigned i = item_results.size(); i > new_end;) {
    NGInlineItemResult& rewind = item_results[--i];
    if (rewind.item->Type() == NGInlineItem::kFloating) {
      NGBlockNode float_node(ToLayoutBox(rewind.item->GetLayoutObject()));
      if (!RemoveUnpositionedFloat(unpositioned_floats_, float_node)) {
        // TODO(kojii): We do not have mechanism to remove once positioned
        // floats yet, and that rewinding them may lay it out twice. For now,
        // prohibit rewinding positioned floats. This may results in incorrect
        // layout, but still better than rewinding them.
        new_end = i + 1;
        if (new_end == item_results.size()) {
          UpdatePosition();
          return;
        }
        break;
      }
    }
  }

  if (new_end) {
    // Use |results[new_end - 1].end_offset| because it may have been truncated
    // and may not be equal to |results[new_end].start_offset|.
    MoveToNextOf(item_results[new_end - 1]);
  } else {
    // When rewinding all items, use |results[0].start_offset|.
    const NGInlineItemResult& first_remove = item_results[new_end];
    item_index_ = first_remove.item_index;
    offset_ = first_remove.start_offset;
  }

  item_results.Shrink(new_end);

  trailing_spaces_collapsed_ = false;
  trailing_collapsible_space_.reset();
  SetLineEndFragment(nullptr);
  UpdatePosition();
}

void NGLineBreaker::SetCurrentStyle(const ComputedStyle& style) {
  auto_wrap_ = style.AutoWrap();

  if (auto_wrap_) {
    if (UNLIKELY(override_break_anywhere_)) {
      break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
    } else {
      switch (style.WordBreak()) {
        case EWordBreak::kNormal:
          break_anywhere_if_overflow_ =
              style.OverflowWrap() == EOverflowWrap::kBreakWord;
          break_iterator_.SetBreakType(LineBreakType::kNormal);
          break;
        case EWordBreak::kBreakAll:
          break_anywhere_if_overflow_ = false;
          break_iterator_.SetBreakType(LineBreakType::kBreakAll);
          break;
        case EWordBreak::kBreakWord:
          break_anywhere_if_overflow_ = true;
          break_iterator_.SetBreakType(LineBreakType::kNormal);
          break;
        case EWordBreak::kKeepAll:
          break_anywhere_if_overflow_ = false;
          break_iterator_.SetBreakType(LineBreakType::kKeepAll);
          break;
      }
    }

    enable_soft_hyphen_ = style.GetHyphens() != Hyphens::kNone;
    hyphenation_ = style.GetHyphenation();
  }

  // The above calls are cheap & necessary. But the following are expensive
  // and do not need to be reset every time if the style doesn't change,
  // so avoid them if possible.
  if (&style == current_style_.get())
    return;

  current_style_ = &style;
  if (auto_wrap_)
    break_iterator_.SetLocale(style.LocaleForLineBreakIterator());
  spacing_.SetSpacing(style.GetFontDescription());
}

void NGLineBreaker::MoveToNextOf(const NGInlineItem& item) {
  offset_ = item.EndOffset();
  item_index_++;
}

void NGLineBreaker::MoveToNextOf(const NGInlineItemResult& item_result) {
  offset_ = item_result.end_offset;
  item_index_ = item_result.item_index;
  DCHECK(item_result.item);
  if (offset_ == item_result.item->EndOffset())
    item_index_++;
}

scoped_refptr<NGInlineBreakToken> NGLineBreaker::CreateBreakToken(
    const NGLineInfo& line_info) const {
  const Vector<NGInlineItem>& items = Items();
  if (item_index_ >= items.size())
    return NGInlineBreakToken::Create(node_);
  return NGInlineBreakToken::Create(
      node_, current_style_.get(), item_index_, offset_,
      ((is_after_forced_break_ ? NGInlineBreakToken::kIsForcedBreak : 0) |
       (line_info.UseFirstLineStyle() ? NGInlineBreakToken::kUseFirstLineStyle
                                      : 0)));
}

}  // namespace blink
