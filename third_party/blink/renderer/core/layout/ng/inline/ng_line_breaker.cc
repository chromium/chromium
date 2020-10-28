// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_breaker.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_ruby_utils.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
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

inline bool IsAllBreakableSpaces(const String& string,
                                 unsigned start,
                                 unsigned end) {
  DCHECK_GE(end, start);
  return StringView(string, start, end - start)
      .IsAllSpecialCharacters<IsBreakableSpace>();
}

// True if the item is "trailable". Trailable items should be included in the
// line if they are after the soft wrap point.
//
// Note that some items are ambiguous; e.g., text is trailable if it has leading
// spaces, and open tags are trailable if spaces follow. This function returns
// true for such cases.
inline bool IsTrailableItemType(NGInlineItem::NGInlineItemType type) {
  return type != NGInlineItem::kAtomicInline &&
         type != NGInlineItem::kOutOfFlowPositioned &&
         type != NGInlineItem::kListMarker;
}

inline bool CanBreakAfterLast(const NGInlineItemResults& item_results) {
  return !item_results.IsEmpty() && item_results.back().can_break_after;
}

inline bool ShouldCreateLineBox(const NGInlineItemResults& item_results) {
  return !item_results.IsEmpty() && item_results.back().should_create_line_box;
}

inline bool HasUnpositionedFloats(const NGInlineItemResults& item_results) {
  return !item_results.IsEmpty() && item_results.back().has_unpositioned_floats;
}

LayoutUnit ComputeInlineEndSize(const NGConstraintSpace& space,
                                const ComputedStyle* style) {
  DCHECK(style);
  NGBoxStrut margins = ComputeMarginsForSelf(space, *style);
  NGBoxStrut borders = ComputeBordersForInline(*style);
  NGBoxStrut paddings = ComputePadding(space, *style);

  return margins.inline_end + borders.inline_end + paddings.inline_end;
}

bool NeedsAccurateEndPosition(const NGInlineItem& line_end_item) {
  DCHECK(line_end_item.Type() == NGInlineItem::kText ||
         line_end_item.Type() == NGInlineItem::kControl);
  DCHECK(line_end_item.Style());
  const ComputedStyle& line_end_style = *line_end_item.Style();
  return line_end_style.HasBoxDecorationBackground() ||
         !line_end_style.AppliedTextDecorations().IsEmpty();
}

inline bool NeedsAccurateEndPosition(const NGLineInfo& line_info,
                                     const NGInlineItem& line_end_item) {
  return line_info.NeedsAccurateEndPosition() ||
         NeedsAccurateEndPosition(line_end_item);
}

inline void ComputeCanBreakAfter(NGInlineItemResult* item_result,
                                 bool auto_wrap,
                                 const LazyLineBreakIterator& break_iterator) {
  item_result->can_break_after =
      auto_wrap && break_iterator.IsBreakable(item_result->EndOffset());
}

inline void RemoveLastItem(NGLineInfo* line_info) {
  NGInlineItemResults* item_results = line_info->MutableResults();
  DCHECK_GT(item_results->size(), 0u);
  item_results->Shrink(item_results->size() - 1);
}

// To correctly determine if a float is allowed to be on the same line as its
// content, we need to determine if it has any ancestors with inline-end
// padding, border, or margin.
// The inline-end size from all of these ancestors contribute to the "used
// size" of the float, and may cause the float to be pushed down.
LayoutUnit ComputeFloatAncestorInlineEndSize(const NGConstraintSpace& space,
                                             const Vector<NGInlineItem>& items,
                                             wtf_size_t item_index) {
  LayoutUnit inline_end_size;
  for (const NGInlineItem *cur = items.begin() + item_index, *end = items.end();
       cur != end; ++cur) {
    const NGInlineItem& item = *cur;

    if (item.Type() == NGInlineItem::kCloseTag) {
      if (item.HasEndEdge())
        inline_end_size += ComputeInlineEndSize(space, item.Style());
      continue;
    }

    // For this calculation, any open tag (even if its empty) stops this
    // calculation, and allows the float to appear on the same line. E.g.
    // <span style="padding-right: 20px;"><f></f><span></span></span>
    //
    // Any non-empty item also allows the float to be on the same line.
    if (item.Type() == NGInlineItem::kOpenTag || !item.IsEmptyItem())
      break;
  }
  return inline_end_size;
}

}  // namespace

inline void NGLineBreaker::ClearNeedsLayout(const NGInlineItem& item) {
  if (mode_ != NGLineBreakerMode::kContent)
    return;
  LayoutObject* layout_object = item.GetLayoutObject();
  if (layout_object->NeedsLayout())
    layout_object->ClearNeedsLayout();
}

LayoutUnit NGLineBreaker::ComputeAvailableWidth() const {
  LayoutUnit available_width = line_opportunity_.AvailableInlineSize();
  // Make sure it's at least the initial size, which is usually 0 but not so
  // when `box-decoration-break: clone`.
  available_width =
      std::max(available_width, cloned_box_decorations_initial_size_);
  // Available width must be smaller than |LayoutUnit::Max()| so that the
  // position can be larger.
  available_width = std::min(available_width, LayoutUnit::NearlyMax());
  return available_width;
}

NGLineBreaker::NGLineBreaker(NGInlineNode node,
                             NGLineBreakerMode mode,
                             const NGConstraintSpace& space,
                             const NGLineLayoutOpportunity& line_opportunity,
                             const NGPositionedFloatVector& leading_floats,
                             unsigned handled_leading_floats_index,
                             const NGInlineBreakToken* break_token,
                             NGExclusionSpace* exclusion_space)
    : line_opportunity_(line_opportunity),
      node_(node),
      mode_(mode),
      is_first_formatted_line_((!break_token || (!break_token->ItemIndex() &&
                                                 !break_token->TextOffset())) &&
                               node.CanContainFirstFormattedLine()),
      use_first_line_style_(is_first_formatted_line_ &&
                            node.UseFirstLineStyle()),
      sticky_images_quirk_(mode != NGLineBreakerMode::kContent &&
                           node.IsStickyImagesQuirkForContentSize()),
      items_data_(node.ItemsData(use_first_line_style_)),
      text_content_(
          !sticky_images_quirk_
              ? items_data_.text_content
              : NGInlineNode::TextContentForStickyImagesQuirk(items_data_)),
      constraint_space_(space),
      exclusion_space_(exclusion_space),
      break_token_(break_token),
      break_iterator_(text_content_),
      shaper_(text_content_),
      spacing_(text_content_),
      leading_floats_(leading_floats),
      handled_leading_floats_index_(handled_leading_floats_index),
      base_direction_(node_.BaseDirection()) {
  available_width_ = ComputeAvailableWidth();
  break_iterator_.SetBreakSpace(BreakSpaceType::kAfterSpaceRun);

  if (!break_token)
    return;

  const ComputedStyle* line_initial_style = break_token->Style();
  if (UNLIKELY(!line_initial_style)) {
    // Usually an inline break token has the line initial style, but class C
    // breaks and last-resort breaks require a break token to start from the
    // beginning of the block. In that case, the line is still the first
    // formatted line, and the line initial style should be computed from the
    // containing block.
    DCHECK_EQ(break_token->ItemIndex(), 0u);
    DCHECK_EQ(break_token->TextOffset(), 0u);
    DCHECK(!break_token->IsForcedBreak());
    DCHECK_EQ(item_index_, break_token->ItemIndex());
    DCHECK_EQ(offset_, break_token->TextOffset());
    DCHECK_EQ(is_after_forced_break_, break_token->IsForcedBreak());
    return;
  }

  item_index_ = break_token->ItemIndex();
  offset_ = break_token->TextOffset();
  break_iterator_.SetStartOffset(offset_);
  is_after_forced_break_ = break_token->IsForcedBreak();
  items_data_.AssertOffset(item_index_, offset_);
  SetCurrentStyle(*line_initial_style);
}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGLineBreaker::~NGLineBreaker() = default;

inline NGInlineItemResult* NGLineBreaker::AddItem(const NGInlineItem& item,
                                                  unsigned end_offset,
                                                  NGLineInfo* line_info) {
  DCHECK_GE(offset_, item.StartOffset());
  DCHECK_GE(end_offset, offset_);
  DCHECK_LE(end_offset, item.EndOffset());
  NGInlineItemResults* item_results = line_info->MutableResults();
  return &item_results->emplace_back(
      &item, item_index_, NGTextOffset(offset_, end_offset),
      break_anywhere_if_overflow_, ShouldCreateLineBox(*item_results),
      HasUnpositionedFloats(*item_results));
}

inline NGInlineItemResult* NGLineBreaker::AddItem(const NGInlineItem& item,
                                                  NGLineInfo* line_info) {
  return AddItem(item, item.EndOffset(), line_info);
}

// Call |HandleOverflow()| if the position is beyond the available space.
inline bool NGLineBreaker::HandleOverflowIfNeeded(NGLineInfo* line_info) {
  if (state_ == LineBreakState::kContinue && !CanFitOnLine()) {
    HandleOverflow(line_info);
    return true;
  }
  return false;
}

void NGLineBreaker::SetIntrinsicSizeOutputs(
    MaxSizeCache* max_size_cache,
    bool* depends_on_percentage_block_size_out) {
  DCHECK_NE(mode_, NGLineBreakerMode::kContent);
  DCHECK(max_size_cache);
  max_size_cache_ = max_size_cache;
  depends_on_percentage_block_size_out_ = depends_on_percentage_block_size_out;
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

void NGLineBreaker::RecalcClonedBoxDecorations() {
  cloned_box_decorations_count_ = 0u;
  cloned_box_decorations_initial_size_ = LayoutUnit();
  cloned_box_decorations_end_size_ = LayoutUnit();
  has_cloned_box_decorations_ = false;

  // Compute which tags are not closed at |item_index_|.
  NGInlineItemsData::OpenTagItems open_items;
  items_data_.GetOpenTagItems(item_index_, &open_items);

  for (const NGInlineItem* item : open_items) {
    if (item->Style()->BoxDecorationBreak() == EBoxDecorationBreak::kClone) {
      has_cloned_box_decorations_ = true;
      ++cloned_box_decorations_count_;
      NGInlineItemResult item_result;
      ComputeOpenTagResult(*item, constraint_space_, &item_result);
      cloned_box_decorations_initial_size_ += item_result.inline_size;
      cloned_box_decorations_end_size_ += item_result.margins.inline_end +
                                          item_result.borders.inline_end +
                                          item_result.padding.inline_end;
    }
  }
  // Advance |position_| by the initial size so that the tab position can
  // accommodate cloned box decorations.
  position_ += cloned_box_decorations_initial_size_;
  // |cloned_box_decorations_initial_size_| may affect available width.
  available_width_ = ComputeAvailableWidth();
  DCHECK_GE(available_width_, cloned_box_decorations_initial_size_);
}

// Add a hyphen string to the |NGInlineItemResult|.
//
// This function changes |NGInlineItemResult::inline_size|, but does not change
// |position_|
LayoutUnit NGLineBreaker::AddHyphen(NGInlineItemResults* item_results,
                                    wtf_size_t index,
                                    NGInlineItemResult* item_result,
                                    const NGInlineItem& item) {
  DCHECK(!HasHyphen());
  DCHECK_EQ(index,
            static_cast<wtf_size_t>(item_result - item_results->begin()));
  DCHECK_LT(index, item_results->size());
  hyphen_index_ = index;

  if (!item_result->hyphen_string) {
    DCHECK(!item_result->hyphen_shape_result);
    DCHECK(item.Style());
    const ComputedStyle& style = *item.Style();
    DCHECK(!item_result->hyphen_string);
    item_result->hyphen_string = style.HyphenString();
    HarfBuzzShaper shaper(item_result->hyphen_string);
    item_result->hyphen_shape_result =
        shaper.Shape(&style.GetFont(), style.Direction());
    has_any_hyphens_ = true;
  }
  DCHECK(item_result->hyphen_string);
  DCHECK(item_result->hyphen_shape_result);
  DCHECK(has_any_hyphens_);

  const LayoutUnit hyphen_inline_size = item_result->HyphenInlineSize();
  item_result->inline_size += hyphen_inline_size;
  return hyphen_inline_size;
}

LayoutUnit NGLineBreaker::AddHyphen(NGInlineItemResults* item_results,
                                    wtf_size_t index) {
  NGInlineItemResult* item_result = &(*item_results)[index];
  DCHECK(item_result->item);
  return AddHyphen(item_results, index, item_result, *item_result->item);
}

LayoutUnit NGLineBreaker::AddHyphen(NGInlineItemResults* item_results,
                                    NGInlineItemResult* item_result,
                                    const NGInlineItem& item) {
  return AddHyphen(item_results, item_result - item_results->begin(),
                   item_result, item);
}

// Remove the hyphen string from the |NGInlineItemResult|.
//
// This function changes |NGInlineItemResult::inline_size|, but does not change
// |position_|
LayoutUnit NGLineBreaker::RemoveHyphen(NGInlineItemResults* item_results) {
  DCHECK(HasHyphen());
  NGInlineItemResult* item_result = &(*item_results)[*hyphen_index_];
  DCHECK(item_result->hyphen_string);
  DCHECK(item_result->hyphen_shape_result);
  const LayoutUnit hyphen_inline_size = item_result->HyphenInlineSize();
  item_result->inline_size -= hyphen_inline_size;
  // |hyphen_string| and |hyphen_shape_result| may be reused when rewinded.
  hyphen_index_.reset();
  return hyphen_inline_size;
}

// Add a hyphen string to the last inflow item in |item_results| if it is
// hyphenated. This can restore the hyphenation state after rewind.
void NGLineBreaker::RestoreLastHyphen(NGInlineItemResults* item_results) {
  DCHECK(!hyphen_index_);
  DCHECK(has_any_hyphens_);
  for (NGInlineItemResult& item_result : base::Reversed(*item_results)) {
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item_result.hyphen_string) {
      AddHyphen(item_results, &item_result, item);
      return;
    }
    if (item.Type() == NGInlineItem::kText ||
        item.Type() == NGInlineItem::kAtomicInline)
      return;
  }
}

// Set the final hyphenation results to |item_results|.
void NGLineBreaker::FinalizeHyphen(NGInlineItemResults* item_results) {
  DCHECK(HasHyphen());
  NGInlineItemResult* item_result = &(*item_results)[*hyphen_index_];
  DCHECK(item_result->hyphen_string);
  DCHECK(item_result->hyphen_shape_result);
  item_result->is_hyphenated = true;
}

// Initialize internal states for the next line.
void NGLineBreaker::PrepareNextLine(NGLineInfo* line_info) {
  // NGLineInfo is not supposed to be re-used because it's not much gain and to
  // avoid rare code path.
  const NGInlineItemResults& item_results = line_info->Results();
  DCHECK(item_results.IsEmpty());

  if (item_index_) {
    // We're past the first line
    previous_line_had_forced_break_ = is_after_forced_break_;
    is_after_forced_break_ = false;
    is_first_formatted_line_ = false;
    use_first_line_style_ = false;
  }

  line_info->SetStartOffset(offset_);
  line_info->SetLineStyle(node_, items_data_, use_first_line_style_);

  DCHECK(!line_info->TextIndent());
  if (line_info->LineStyle().ShouldUseTextIndent(
          is_first_formatted_line_, previous_line_had_forced_break_)) {
    const Length& length = line_info->LineStyle().TextIndent();
    LayoutUnit maximum_value;
    // Ignore percentages (resolve to 0) when calculating min/max intrinsic
    // sizes.
    if (length.IsPercentOrCalc() && mode_ == NGLineBreakerMode::kContent)
      maximum_value = constraint_space_.AvailableSize().inline_size;
    line_info->SetTextIndent(MinimumValueForLength(length, maximum_value));
  }

  // Set the initial style of this line from the line style, if the style from
  // the end of previous line is not available. Example:
  //   <p>...<span>....</span></p>
  // When the line wraps in <span>, the 2nd line needs to start with the style
  // of the <span>.
  if (!current_style_)
    SetCurrentStyle(line_info->LineStyle());
  ComputeBaseDirection();
  line_info->SetBaseDirection(base_direction_);
  hyphen_index_.reset();
  has_any_hyphens_ = false;

  // Use 'text-indent' as the initial position. This lets tab positions to align
  // regardless of 'text-indent'.
  position_ = line_info->TextIndent();

  has_cloned_box_decorations_ = false;
  if (UNLIKELY((break_token_ && break_token_->HasClonedBoxDecorations()) ||
               cloned_box_decorations_count_))
    RecalcClonedBoxDecorations();

  ResetRewindLoopDetector();
}

void NGLineBreaker::NextLine(
    LayoutUnit percentage_resolution_block_size_for_min_max,
    NGLineInfo* line_info) {
  PrepareNextLine(line_info);
  BreakLine(percentage_resolution_block_size_for_min_max, line_info);
  if (UNLIKELY(HasHyphen()))
    FinalizeHyphen(line_info->MutableResults());
  RemoveTrailingCollapsibleSpace(line_info);

  const NGInlineItemResults& item_results = line_info->Results();
#if DCHECK_IS_ON()
  for (const auto& result : item_results)
    result.CheckConsistency(mode_ == NGLineBreakerMode::kMinContent);
#endif

  // We should create a line-box when:
  //  - We have an item which needs a line box (text, etc).
  //  - A list-marker is present, and it would be the last line or last line
  //    before a forced new-line.
  //  - During min/max content sizing (to correctly determine the line width).
  //
  // TODO(kojii): There are cases where we need to PlaceItems() without creating
  // line boxes. These cases need to be reviewed.
  bool should_create_line_box = ShouldCreateLineBox(item_results) ||
                                (has_list_marker_ && line_info->IsLastLine()) ||
                                mode_ != NGLineBreakerMode::kContent ||
                                node_.HasLineEvenIfEmpty();

  if (!should_create_line_box)
    line_info->SetIsEmptyLine();
  line_info->SetEndItemIndex(item_index_);
  DCHECK_NE(trailing_whitespace_, WhitespaceState::kUnknown);
  if (trailing_whitespace_ == WhitespaceState::kPreserved)
    line_info->SetHasTrailingSpaces();

  ComputeLineLocation(line_info);
}

void NGLineBreaker::BreakLine(
    LayoutUnit percentage_resolution_block_size_for_min_max,
    NGLineInfo* line_info) {
  DCHECK(!line_info->IsLastLine());
  const Vector<NGInlineItem>& items = Items();
  state_ = LineBreakState::kContinue;
  trailing_whitespace_ = WhitespaceState::kLeading;
  while (state_ != LineBreakState::kDone) {
    // If we reach at the end of the block, this is the last line.
    DCHECK_LE(item_index_, items.size());
    if (item_index_ == items.size()) {
      // Still check overflow because the last item may have overflowed.
      if (HandleOverflowIfNeeded(line_info) && item_index_ != items.size())
        continue;
      if (UNLIKELY(HasHyphen()))
        position_ -= RemoveHyphen(line_info->MutableResults());
      line_info->SetIsLastLine(true);
      return;
    }

    // If |state_| is overflow, break at the earliest break opportunity.
    const NGInlineItemResults& item_results = line_info->Results();
    if (UNLIKELY(state_ == LineBreakState::kOverflow &&
                 CanBreakAfterLast(item_results))) {
      state_ = LineBreakState::kTrailing;
    }

    // Handle trailable items first. These items may not be break before.
    // They (or part of them) may also overhang the available width.
    const NGInlineItem& item = items[item_index_];
    if (item.Type() == NGInlineItem::kText) {
      if (item.Length())
        HandleText(item, *item.TextShapeResult(), line_info);
      else
        HandleEmptyText(item, line_info);
#if DCHECK_IS_ON()
      if (!item_results.IsEmpty())
        item_results.back().CheckConsistency(true);
#endif
      continue;
    }
    if (item.Type() == NGInlineItem::kOpenTag) {
      HandleOpenTag(item, line_info);
      continue;
    }
    if (item.Type() == NGInlineItem::kCloseTag) {
      HandleCloseTag(item, line_info);
      continue;
    }
    if (item.Type() == NGInlineItem::kControl) {
      HandleControlItem(item, line_info);
      continue;
    }
    if (item.Type() == NGInlineItem::kFloating) {
      HandleFloat(item, line_info);
      continue;
    }
    if (item.Type() == NGInlineItem::kBidiControl) {
      HandleBidiControlItem(item, line_info);
      continue;
    }

    // Items after this point are not trailable. If we're trailing, break before
    // any non-trailable items
    DCHECK(!IsTrailableItemType(item.Type()));
    if (state_ == LineBreakState::kTrailing) {
      DCHECK(!line_info->IsLastLine());
      return;
    }

    if (item.Type() == NGInlineItem::kAtomicInline) {
      HandleAtomicInline(item, percentage_resolution_block_size_for_min_max,
                         line_info);
      continue;
    }
    if (item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      HandleOutOfFlowPositioned(item, line_info);
    } else if (item.Length()) {
      NOTREACHED();
      // For other items with text (e.g., bidi controls), use their text to
      // determine the break opportunity.
      NGInlineItemResult* item_result = AddItem(item, line_info);
      item_result->can_break_after =
          break_iterator_.IsBreakable(item_result->EndOffset());
      MoveToNextOf(item);
    } else if (item.Type() == NGInlineItem::kListMarker) {
      NGInlineItemResult* item_result = AddItem(item, line_info);
      has_list_marker_ = true;
      DCHECK(!item_result->can_break_after);
      MoveToNextOf(item);
    } else {
      NOTREACHED();
      MoveToNextOf(item);
    }
  }
}

void NGLineBreaker::ComputeLineLocation(NGLineInfo* line_info) const {
  // Negative margins can make the position negative, but the inline size is
  // always positive or 0.
  LayoutUnit available_width = AvailableWidth();
  line_info->SetWidth(available_width,
                      position_ + cloned_box_decorations_end_size_);
  line_info->SetBfcOffset(
      {line_opportunity_.line_left_offset, line_opportunity_.bfc_block_offset});
  if (mode_ == NGLineBreakerMode::kContent)
    line_info->UpdateTextAlign();
}

// Atomic inlines have break opportunities before and after, even when the
// adjacent character is U+00A0 NO-BREAK SPACE character.
bool NGLineBreaker::ShouldForceCanBreakAfter(
    const NGInlineItemResult& item_result) const {
  DCHECK(auto_wrap_);
  DCHECK_EQ(item_result.item->Type(), NGInlineItem::kText);
  const String& text = Text();
  DCHECK_GE(text.length(), item_result.EndOffset());
  if (text.length() <= item_result.EndOffset() ||
      text[item_result.EndOffset()] != kObjectReplacementCharacter)
    return false;
  // This kObjectReplacementCharacter can be any objects, such as a floating or
  // an OOF object. Check if it's really an atomic inline.
  const Vector<NGInlineItem>& items = Items();
  for (const NGInlineItem* item = std::next(item_result.item);
       item != items.end(); ++item) {
    DCHECK_EQ(item->StartOffset(), item_result.EndOffset());
    if (item->Type() == NGInlineItem::kAtomicInline) {
      // Except when sticky images quirk was applied.
      if (UNLIKELY(text[item->StartOffset()] == kNoBreakSpaceCharacter))
        return false;
      return !item->IsRubyRun();
    }
    if (item->EndOffset() > item_result.EndOffset())
      break;
  }
  return false;
}

void NGLineBreaker::HandleText(const NGInlineItem& item,
                               const ShapeResult& shape_result,
                               NGLineInfo* line_info) {
  DCHECK(item.Type() == NGInlineItem::kText ||
         (item.Type() == NGInlineItem::kControl &&
          Text()[item.StartOffset()] == kTabulationCharacter));
  DCHECK(&shape_result);
  DCHECK_EQ(auto_wrap_, item.Style()->AutoWrap());

  // If we're trailing, only trailing spaces can be included in this line.
  if (UNLIKELY(state_ == LineBreakState::kTrailing)) {
    HandleTrailingSpaces(item, &shape_result, line_info);
    return;
  }

  // Skip leading collapsible spaces.
  // Most cases such spaces are handled as trailing spaces of the previous line,
  // but there are some cases doing so is too complex.
  if (trailing_whitespace_ == WhitespaceState::kLeading) {
    if (item.Style()->CollapseWhiteSpace() &&
        Text()[offset_] == kSpaceCharacter) {
      // Skipping one whitespace removes all collapsible spaces because
      // collapsible spaces are collapsed to single space in
      // NGInlineItemBuilder.
      ++offset_;
      if (offset_ == item.EndOffset()) {
        ClearNeedsLayout(item);
        MoveToNextOf(item);
        return;
      }
    }
    // |trailing_whitespace_| will be updated as we read the text.
  }

  // Go to |HandleOverflow()| if the last item overflowed, and we're adding
  // text.
  if (state_ == LineBreakState::kContinue && !CanFitOnLine()) {
    // |HandleOverflow()| expects all trailable items are added. If this text
    // starts with trailable spaces, add them. TODO(kojii): This can be
    // optimzied further. This is necesasry only if |HandleOverflow()| does not
    // rewind, but in most cases it will rewind.
    const String& text = Text();
    if (auto_wrap_ && IsBreakableSpace(text[offset_])) {
      HandleTrailingSpaces(item, &shape_result, line_info);
      if (state_ != LineBreakState::kDone) {
        state_ = LineBreakState::kContinue;
        return;
      }
    }
    HandleOverflow(line_info);
    return;
  }

  if (UNLIKELY(HasHyphen()))
    position_ -= RemoveHyphen(line_info->MutableResults());

  NGInlineItemResult* item_result = AddItem(item, line_info);
  item_result->should_create_line_box = true;
  // Try to commit |pending_end_overhang_| of a prior NGInlineItemResult.
  // |pending_end_overhang_| doesn't work well with bidi reordering. It's
  // difficult to compute overhang after bidi reordering because it affect
  // line breaking.
  if (maybe_have_end_overhang_)
    position_ -= CommitPendingEndOverhang(line_info);

  if (auto_wrap_) {
    if (mode_ == NGLineBreakerMode::kMinContent &&
        HandleTextForFastMinContent(item_result, item, shape_result,
                                    line_info)) {
      return;
    }

    // Try to break inside of this text item.
    const LayoutUnit available_width = RemainingAvailableWidth();
    BreakResult break_result =
        BreakText(item_result, item, shape_result, available_width,
                  available_width, line_info);
    DCHECK(item_result->shape_result ||
           (break_result == kOverflow && break_anywhere_if_overflow_ &&
            !override_break_anywhere_));
    position_ += item_result->inline_size;
    DCHECK_EQ(break_result == kSuccess, CanFitOnLine());
    MoveToNextOf(*item_result);

    if (break_result == kSuccess) {
      DCHECK(item_result->shape_result);

      // If the break is at the middle of a text item, we know no trailable
      // items follow, only trailable spaces if any. This is very common that
      // shortcut to handling trailing spaces.
      if (item_result->EndOffset() < item.EndOffset())
        return HandleTrailingSpaces(item, &shape_result, line_info);

      // The break point found at the end of this text item. Continue looking
      // next items, because the next item maybe trailable, or can prohibit
      // breaking before.
      return;
    }
    DCHECK_EQ(break_result, kOverflow);

    // Handle `overflow-wrap` if it is enabled and if this text item overflows.
    if (UNLIKELY(!item_result->shape_result)) {
      DCHECK(break_anywhere_if_overflow_ && !override_break_anywhere_);
      HandleOverflow(line_info);
      return;
    }

    // If we're seeking for the first break opportunity, update the state.
    if (UNLIKELY(state_ == LineBreakState::kOverflow)) {
      if (item_result->can_break_after)
        state_ = LineBreakState::kTrailing;
      return;
    }

    // Hanging trailing spaces may resolve the overflow.
    if (item_result->has_only_trailing_spaces) {
      state_ = LineBreakState::kTrailing;
      if (item_result->item->Style()->WhiteSpace() == EWhiteSpace::kPreWrap &&
          IsBreakableSpace(Text()[item_result->EndOffset() - 1])) {
        unsigned end_index = item_result - line_info->Results().begin();
        Rewind(end_index, line_info);
      }
      return;
    }

    // If this is all trailable spaces, this item is trailable, and next item
    // maybe too. Don't go to |HandleOverflow()| yet.
    if (IsAllBreakableSpaces(Text(), item_result->StartOffset(),
                             item_result->EndOffset()))
      return;

    HandleOverflow(line_info);
    return;
  }

  // Add until the end of the item if !auto_wrap. In most cases, it's the whole
  // item.
  DCHECK_EQ(item_result->EndOffset(), item.EndOffset());
  if (item_result->StartOffset() == item.StartOffset()) {
    item_result->inline_size =
        shape_result.SnappedWidth().ClampNegativeToZero();
    item_result->shape_result = ShapeResultView::Create(&shape_result);
  } else {
    // <wbr> can wrap even if !auto_wrap. Spaces after that will be leading
    // spaces and thus be collapsed.
    DCHECK(trailing_whitespace_ == WhitespaceState::kLeading &&
           item_result->StartOffset() >= item.StartOffset());
    item_result->shape_result = ShapeResultView::Create(
        &shape_result, item_result->StartOffset(), item_result->EndOffset());
    item_result->inline_size =
        item_result->shape_result->SnappedWidth().ClampNegativeToZero();
  }

  DCHECK(!item_result->may_break_inside);
  DCHECK(!item_result->can_break_after);
  trailing_whitespace_ = WhitespaceState::kUnknown;
  position_ += item_result->inline_size;
  MoveToNextOf(item);
}

NGLineBreaker::BreakResult NGLineBreaker::BreakText(
    NGInlineItemResult* item_result,
    const NGInlineItem& item,
    const ShapeResult& item_shape_result,
    LayoutUnit available_width,
    LayoutUnit available_width_with_hyphens,
    NGLineInfo* line_info) {
  DCHECK(item.Type() == NGInlineItem::kText ||
         (item.Type() == NGInlineItem::kControl &&
          Text()[item.StartOffset()] == kTabulationCharacter));
  DCHECK(&item_shape_result);
  item.AssertOffset(item_result->StartOffset());

  // The hyphenation state should be cleared before the entry. This function
  // may reset it, but this function cannot determine whether it should update
  // |position_| or not.
  DCHECK(!HasHyphen());

  DCHECK_EQ(item_shape_result.StartIndex(), item.StartOffset());
  DCHECK_EQ(item_shape_result.EndIndex(), item.EndOffset());
  struct ShapeCallbackContext {
    STACK_ALLOCATED();

   public:
    NGLineBreaker* line_breaker;
    const NGInlineItem& item;
  } shape_callback_context{this, item};
  const ShapingLineBreaker::ShapeCallback shape_callback =
      [](void* untyped_context, unsigned start, unsigned end) {
        ShapeCallbackContext* context =
            static_cast<ShapeCallbackContext*>(untyped_context);
        return context->line_breaker->ShapeText(context->item, start, end);
      };
  ShapingLineBreaker breaker(&item_shape_result, &break_iterator_, hyphenation_,
                             shape_callback, &shape_callback_context);
  if (!enable_soft_hyphen_)
    breaker.DisableSoftHyphen();

  // Use kStartShouldBeSafe if at the beginning of a line.
  unsigned options = ShapingLineBreaker::kDefaultOptions;
  if (item_result->StartOffset() != line_info->StartOffset())
    options |= ShapingLineBreaker::kDontReshapeStart;

  // Reshaping between the last character and trailing spaces is needed only
  // when we need accurate end position, because kerning between trailing spaces
  // is not visible.
  if (!NeedsAccurateEndPosition(*line_info, item))
    options |= ShapingLineBreaker::kDontReshapeEndIfAtSpace;

  // Use kNoResultIfOverflow if 'break-word' and we're trying to break normally
  // because if this item overflows, we will rewind and break line again. The
  // overflowing ShapeResult is not needed.
  if (break_anywhere_if_overflow_ && !override_break_anywhere_)
    options |= ShapingLineBreaker::kNoResultIfOverflow;

#if DCHECK_IS_ON()
  unsigned try_count = 0;
#endif
  LayoutUnit inline_size;
  ShapingLineBreaker::Result result;
  while (true) {
#if DCHECK_IS_ON()
    ++try_count;
    DCHECK_LE(try_count, 2u);
#endif
    scoped_refptr<const ShapeResultView> shape_result = breaker.ShapeLine(
        item_result->StartOffset(), available_width.ClampNegativeToZero(),
        options, &result);

    // If this item overflows and 'break-word' is set, this line will be
    // rewinded. Making this item long enough to overflow is enough.
    if (!shape_result) {
      DCHECK(options & ShapingLineBreaker::kNoResultIfOverflow);
      item_result->inline_size = available_width_with_hyphens + 1;
      item_result->text_offset.end = item.EndOffset();
      item_result->text_offset.AssertNotEmpty();
      return kOverflow;
    }
    DCHECK_EQ(shape_result->NumCharacters(),
              result.break_offset - item_result->StartOffset());
    // It is critical to move the offset forward, or NGLineBreaker may keep
    // adding NGInlineItemResult until all the memory is consumed.
    CHECK_GT(result.break_offset, item_result->StartOffset());

    inline_size = shape_result->SnappedWidth().ClampNegativeToZero();
    item_result->inline_size = inline_size;
    if (UNLIKELY(result.is_hyphenated)) {
      NGInlineItemResults* item_results = line_info->MutableResults();
      const LayoutUnit hyphen_inline_size =
          AddHyphen(item_results, item_result, item);
      // If the hyphen overflows, retry with the reduced available width.
      if (!result.is_overflow && inline_size <= available_width) {
        const LayoutUnit space_for_hyphen =
            available_width_with_hyphens - inline_size;
        if (space_for_hyphen >= 0 && hyphen_inline_size > space_for_hyphen) {
          available_width -= hyphen_inline_size;
          RemoveHyphen(item_results);
          continue;
        }
      }
      inline_size = item_result->inline_size;
    }
    item_result->text_offset.end = result.break_offset;
    item_result->text_offset.AssertNotEmpty();
    item_result->non_hangable_run_end = result.non_hangable_run_end;
    item_result->has_only_trailing_spaces = result.has_trailing_spaces;
    item_result->shape_result = std::move(shape_result);
    break;
  }

  // * If width <= available_width:
  //   * If offset < item.EndOffset(): the break opportunity to fit is found.
  //   * If offset == item.EndOffset(): the break opportunity at the end fits,
  //     or the first break opportunity is beyond the end.
  //     There may be room for more characters.
  // * If width > available_width: The first break opportunity does not fit.
  //   offset is the first break opportunity, either inside, at the end, or
  //   beyond the end.
  if (item_result->EndOffset() < item.EndOffset()) {
    item_result->can_break_after = true;

    if (UNLIKELY(break_iterator_.BreakType() ==
                 LineBreakType::kBreakCharacter)) {
      trailing_whitespace_ = WhitespaceState::kUnknown;
    } else {
      trailing_whitespace_ = WhitespaceState::kNone;
    }
  } else {
    DCHECK_EQ(item_result->EndOffset(), item.EndOffset());
    item_result->can_break_after =
        break_iterator_.IsBreakable(item_result->EndOffset());
    if (!item_result->can_break_after && item.Type() == NGInlineItem::kText &&
        ShouldForceCanBreakAfter(*item_result))
      item_result->can_break_after = true;
    trailing_whitespace_ = WhitespaceState::kUnknown;
  }

  // This result is not breakable any further if overflow. This information is
  // useful to optimize |HandleOverflow()|.
  item_result->may_break_inside = !result.is_overflow;

  // TODO(crbug.com/1003742): We should use |result.is_overflow| here. For now,
  // use |inline_size| because some tests rely on this behavior.
  return inline_size <= available_width ? kSuccess : kOverflow;
}

// Breaks the text item at the previous break opportunity from
// |item_result->text_offset.end|. Returns false if there were no previous break
// opportunities.
bool NGLineBreaker::BreakTextAtPreviousBreakOpportunity(
    NGInlineItemResult* item_result) {
  DCHECK(item_result->item);
  DCHECK(item_result->may_break_inside);
  const NGInlineItem& item = *item_result->item;
  DCHECK_EQ(item.Type(), NGInlineItem::kText);
  DCHECK(item.Style() && item.Style()->AutoWrap());

  // TODO(jfernandez): Should we use the non-hangable-run-end instead ?
  unsigned break_opportunity = break_iterator_.PreviousBreakOpportunity(
      item_result->EndOffset() - 1, item_result->StartOffset());
  if (break_opportunity <= item_result->StartOffset())
    return false;
  item_result->text_offset.end = break_opportunity;
  item_result->text_offset.AssertNotEmpty();
  item_result->shape_result = ShapeResultView::Create(
      item.TextShapeResult(), item_result->StartOffset(),
      item_result->EndOffset());
  item_result->inline_size =
      item_result->shape_result->SnappedWidth().ClampNegativeToZero();
  item_result->can_break_after = true;

  if (trailing_collapsible_space_.has_value() &&
      trailing_collapsible_space_->item_result == item_result) {
    trailing_collapsible_space_.reset();
  }

  return true;
}

// This function handles text item for min-content. The specialized logic is
// because min-content is very expensive by breaking at every break opportunity
// and producing as many lines as the number of break opportunities.
//
// This function breaks the text in NGInlineItem at every break opportunity,
// computes the maximum width of all words, and creates one NGInlineItemResult
// that has the maximum width. For example, for a text item of "1 2 34 5 6",
// only the width of "34" matters for min-content.
//
// The first word and the last word, "1" and "6" in the example above, are
// handled in normal |HandleText()| because they may form a word with the
// previous/next item.
bool NGLineBreaker::HandleTextForFastMinContent(NGInlineItemResult* item_result,
                                                const NGInlineItem& item,
                                                const ShapeResult& shape_result,
                                                NGLineInfo* line_info) {
  DCHECK_EQ(mode_, NGLineBreakerMode::kMinContent);
  DCHECK(auto_wrap_);
  DCHECK(item.Type() == NGInlineItem::kText ||
         (item.Type() == NGInlineItem::kControl &&
          Text()[item.StartOffset()] == kTabulationCharacter));
  DCHECK(&shape_result);

  // If this is the first part of the text, it may form a word with the previous
  // item. Fallback to |HandleText()|.
  unsigned start_offset = item_result->StartOffset();
  DCHECK_LT(start_offset, item.EndOffset());
  if (start_offset != line_info->StartOffset() &&
      start_offset == item.StartOffset())
    return false;
  // If this is the last part of the text, it may form a word with the next
  // item. Fallback to |HandleText()|.
  if (fast_min_content_item_ == &item)
    return false;

  // Hyphenation is not supported yet.
  if (hyphenation_)
    return false;

  base::Optional<LineBreakType> saved_line_break_type;
  if (break_anywhere_if_overflow_ && !override_break_anywhere_) {
    saved_line_break_type = break_iterator_.BreakType();
    break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
  }

  // Break the text at every break opportunity and measure each word.
  DCHECK_EQ(shape_result.StartIndex(), item.StartOffset());
  DCHECK_GE(start_offset, shape_result.StartIndex());
  shape_result.EnsurePositionData();
  const String& text = Text();
  float min_width = 0;
  unsigned last_end_offset = 0;
  while (start_offset < item.EndOffset()) {
    unsigned end_offset = break_iterator_.NextBreakOpportunity(
        start_offset + 1, item.EndOffset());
    if (end_offset >= item.EndOffset())
      break;

    unsigned non_hangable_run_end = end_offset;
    if (item.Style()->WhiteSpace() != EWhiteSpace::kBreakSpaces) {
      while (non_hangable_run_end > item.StartOffset() &&
             IsBreakableSpace(text[non_hangable_run_end - 1])) {
        --non_hangable_run_end;
      }
    }

    // Inserting a hyphenation character is not supported yet.
    // TODO (jfernandez): Maybe we need to use 'end_offset' here ?
    if (text[non_hangable_run_end - 1] == kSoftHyphenCharacter)
      return false;

    float start_position = shape_result.CachedPositionForOffset(
        start_offset - shape_result.StartIndex());
    float end_position = shape_result.CachedPositionForOffset(
        non_hangable_run_end - shape_result.StartIndex());
    float word_width = IsLtr(shape_result.Direction())
                           ? end_position - start_position
                           : start_position - end_position;
    min_width = std::max(word_width, min_width);

    last_end_offset = non_hangable_run_end;
    // TODO (jfernandez): I think that having the non_hangable_run_end
    // would make this loop unnecessary/redudant.
    start_offset = end_offset;
    while (start_offset < item.EndOffset() &&
           IsBreakableSpace(text[start_offset])) {
      ++start_offset;
    }
  }

  if (saved_line_break_type.has_value())
    break_iterator_.SetBreakType(*saved_line_break_type);

  // If there was only one break opportunity in this item, it may form a word
  // with previous and/or next item. Fallback to |HandleText()|.
  if (!last_end_offset)
    return false;

  // Create an NGInlineItemResult that has the max of widths of all words.
  item_result->text_offset.end =
      std::max(last_end_offset, item_result->text_offset.start + 1);
  item_result->text_offset.AssertNotEmpty();
  item_result->inline_size = LayoutUnit::FromFloatCeil(min_width);
  item_result->can_break_after = true;

  trailing_whitespace_ = WhitespaceState::kUnknown;
  position_ += item_result->inline_size;
  state_ = LineBreakState::kTrailing;
  fast_min_content_item_ = &item;
  MoveToNextOf(*item_result);
  return true;
}

void NGLineBreaker::HandleEmptyText(const NGInlineItem& item,
                                    NGLineInfo* line_info) {
  // Fully collapsed text is not needed for line breaking/layout, but it may
  // have |SelfNeedsLayout()| set. Mark it was laid out.
  ClearNeedsLayout(item);
  MoveToNextOf(item);
}

// Re-shape the specified range of |NGInlineItem|.
scoped_refptr<ShapeResult> NGLineBreaker::ShapeText(const NGInlineItem& item,
                                                    unsigned start,
                                                    unsigned end) {
  scoped_refptr<ShapeResult> shape_result;
  if (!items_data_.segments) {
    RunSegmenter::RunSegmenterRange segment_range =
        item.CreateRunSegmenterRange();
    shape_result = shaper_.Shape(&item.Style()->GetFont(), item.Direction(),
                                 start, end, segment_range);
  } else {
    shape_result = items_data_.segments->ShapeText(
        &shaper_, &item.Style()->GetFont(), item.Direction(), start, end,
        &item - items_data_.items.begin());
  }
  if (UNLIKELY(spacing_.HasSpacing()))
    shape_result->ApplySpacing(spacing_);
  return shape_result;
}

// Compute a new ShapeResult for the specified end offset.
// The end is re-shaped if it is not safe-to-break.
scoped_refptr<ShapeResultView> NGLineBreaker::TruncateLineEndResult(
    const NGLineInfo& line_info,
    const NGInlineItemResult& item_result,
    unsigned end_offset) {
  DCHECK(item_result.item);
  const NGInlineItem& item = *item_result.item;

  // Check given offsets require to truncate |item_result.shape_result|.
  const unsigned start_offset = item_result.StartOffset();
  const ShapeResultView* source_result = item_result.shape_result.get();
  DCHECK(source_result);
  DCHECK_GE(start_offset, source_result->StartIndex());
  DCHECK_LE(end_offset, source_result->EndIndex());
  DCHECK(start_offset > source_result->StartIndex() ||
         end_offset < source_result->EndIndex());

  if (!NeedsAccurateEndPosition(line_info, item)) {
    return ShapeResultView::Create(source_result, start_offset, end_offset);
  }

  unsigned last_safe = source_result->PreviousSafeToBreakOffset(end_offset);
  DCHECK_LE(last_safe, end_offset);
  if (last_safe == end_offset || last_safe <= start_offset) {
    return ShapeResultView::Create(source_result, start_offset, end_offset);
  }

  scoped_refptr<ShapeResult> end_result =
      ShapeText(item, std::max(last_safe, start_offset), end_offset);
  DCHECK_EQ(end_result->Direction(), source_result->Direction());
  ShapeResultView::Segment segments[2];
  segments[0] = {source_result, start_offset, last_safe};
  segments[1] = {end_result.get(), 0, end_offset};
  return ShapeResultView::Create(&segments[0], 2);
}

// Update |ShapeResult| in |item_result| to match to its |start_offset| and
// |end_offset|. The end is re-shaped if it is not safe-to-break.
void NGLineBreaker::UpdateShapeResult(const NGLineInfo& line_info,
                                      NGInlineItemResult* item_result) {
  DCHECK(item_result);
  item_result->shape_result =
      TruncateLineEndResult(line_info, *item_result, item_result->EndOffset());
  DCHECK(item_result->shape_result);
  item_result->inline_size = item_result->shape_result->SnappedWidth();
}

inline void NGLineBreaker::HandleTrailingSpaces(const NGInlineItem& item,
                                                NGLineInfo* line_info) {
  const ShapeResult* shape_result = item.TextShapeResult();
  // Call |HandleTrailingSpaces| even if |item| does not have |ShapeResult|, so
  // that we skip spaces.
  HandleTrailingSpaces(item, shape_result, line_info);
}

void NGLineBreaker::HandleTrailingSpaces(const NGInlineItem& item,
                                         const ShapeResult* shape_result,
                                         NGLineInfo* line_info) {
  DCHECK(item.Type() == NGInlineItem::kText ||
         (item.Type() == NGInlineItem::kControl &&
          Text()[item.StartOffset()] == kTabulationCharacter));
  bool is_control_tab = item.Type() == NGInlineItem::kControl &&
                        Text()[item.StartOffset()] == kTabulationCharacter;
  DCHECK(item.Type() == NGInlineItem::kText || is_control_tab);
  DCHECK_GE(offset_, item.StartOffset());
  DCHECK_LT(offset_, item.EndOffset());
  const String& text = Text();
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();

  if (!auto_wrap_) {
    state_ = LineBreakState::kDone;
    return;
  }

  if (style.CollapseWhiteSpace()) {
    if (text[offset_] != kSpaceCharacter) {
      if (offset_ > 0 && IsBreakableSpace(text[offset_ - 1]))
        trailing_whitespace_ = WhitespaceState::kCollapsible;
      state_ = LineBreakState::kDone;
      return;
    }

    // Skipping one whitespace removes all collapsible spaces because
    // collapsible spaces are collapsed to single space in NGInlineItemBuilder.
    offset_++;
    trailing_whitespace_ = WhitespaceState::kCollapsed;

    // Make the last item breakable after, even if it was nowrap.
    NGInlineItemResults* item_results = line_info->MutableResults();
    DCHECK(!item_results->IsEmpty());
    item_results->back().can_break_after = true;
  } else if (style.WhiteSpace() != EWhiteSpace::kBreakSpaces) {
    // Find the end of the run of space characters in this item.
    // Other white space characters (e.g., tab) are not included in this item.
    DCHECK(style.BreakOnlyAfterWhiteSpace());
    unsigned end = offset_;
    while (end < item.EndOffset() && IsBreakableSpace(text[end]))
      end++;
    if (end == offset_) {
      if (IsBreakableSpace(text[end - 1]))
        trailing_whitespace_ = WhitespaceState::kPreserved;
      state_ = LineBreakState::kDone;
      return;
    }

    // TODO (jfernandez): Could we just modify the last ItemResult
    // instead of creating a new one ?
    // Probably we can (koji). We would need to review usage of these
    // item results, and change them to use "non_hangable_run_end"
    // instead.
    DCHECK(shape_result);
    NGInlineItemResult* item_result = AddItem(item, end, line_info);
    item_result->should_create_line_box = true;
    item_result->non_hangable_run_end = offset_;
    item_result->has_only_trailing_spaces = true;
    item_result->shape_result = ShapeResultView::Create(shape_result);
    if (item_result->StartOffset() == item.StartOffset() &&
        item_result->EndOffset() == item.EndOffset()) {
      item_result->inline_size = item_result->shape_result
                                     ? item_result->shape_result->SnappedWidth()
                                     : LayoutUnit();
    } else {
      UpdateShapeResult(*line_info, item_result);
    }
    position_ += item_result->inline_size;
    item_result->can_break_after =
        end < text.length() && !IsBreakableSpace(text[end]);
    offset_ = end;
    trailing_whitespace_ = WhitespaceState::kPreserved;
  }

  // If non-space characters follow, the line is done.
  // Otherwise keep checking next items for the break point.
  DCHECK_LE(offset_, item.EndOffset());
  if (offset_ < item.EndOffset()) {
    state_ = LineBreakState::kDone;
    return;
  }
  ClearNeedsLayout(item);
  item_index_++;
  state_ = LineBreakState::kTrailing;
}

// Remove trailing collapsible spaces in |line_info|.
// https://drafts.csswg.org/css-text-3/#white-space-phase-2
void NGLineBreaker::RemoveTrailingCollapsibleSpace(NGLineInfo* line_info) {
  // Remove trailing open tags. Open tags are included as trailable items
  // because they are ambiguous. When the line ends, and if the end of line has
  // open tags, they are not trailable.
  // TODO(crbug.com/1009936): Open tags and trailing space items can interleave,
  // but the current code supports only one trailing space item. Multiple
  // trailing space items and interleaved open/close tags should be supported.
  const NGInlineItemResults& item_results = line_info->Results();
  for (const NGInlineItemResult& item_result : base::Reversed(item_results)) {
    DCHECK(item_result.item);
    if (item_result.item->Type() != NGInlineItem::kOpenTag) {
      unsigned end_index = &item_result - item_results.begin() + 1;
      if (end_index < item_results.size()) {
        const NGInlineItemResult& end_item_result = item_results[end_index];
        unsigned end_item_index = end_item_result.item_index;
        unsigned end_offset = end_item_result.StartOffset();
        ResetRewindLoopDetector();
        Rewind(end_index, line_info);
        item_index_ = end_item_index;
        offset_ = end_offset;
        items_data_.AssertOffset(item_index_, offset_);
      }
      break;
    }
  }

  ComputeTrailingCollapsibleSpace(line_info);
  if (!trailing_collapsible_space_.has_value()) {
    return;
  }

  // We have a trailing collapsible space. Remove it.
  NGInlineItemResult* item_result = trailing_collapsible_space_->item_result;
  position_ -= item_result->inline_size;
  if (scoped_refptr<const ShapeResultView>& collapsed_shape_result =
          trailing_collapsible_space_->collapsed_shape_result) {
    --item_result->text_offset.end;
    item_result->text_offset.AssertNotEmpty();
    item_result->shape_result = collapsed_shape_result;
    item_result->inline_size = item_result->shape_result->SnappedWidth();
    position_ += item_result->inline_size;
  } else {
    ClearNeedsLayout(*item_result->item);
    line_info->MutableResults()->erase(item_result);
  }
  trailing_collapsible_space_.reset();
  trailing_whitespace_ = WhitespaceState::kCollapsed;
}

// Compute the width of trailing spaces without removing it.
LayoutUnit NGLineBreaker::TrailingCollapsibleSpaceWidth(NGLineInfo* line_info) {
  ComputeTrailingCollapsibleSpace(line_info);
  if (!trailing_collapsible_space_.has_value())
    return LayoutUnit();

  // Normally, the width of new_reuslt is smaller, but technically it can be
  // larger. In such case, it means the trailing spaces has negative width.
  NGInlineItemResult* item_result = trailing_collapsible_space_->item_result;
  if (scoped_refptr<const ShapeResultView>& collapsed_shape_result =
          trailing_collapsible_space_->collapsed_shape_result) {
    return item_result->inline_size - collapsed_shape_result->SnappedWidth();
  }
  return item_result->inline_size;
}

// Find trailing collapsible space if exists. The result is cached to
// |trailing_collapsible_space_|.
void NGLineBreaker::ComputeTrailingCollapsibleSpace(NGLineInfo* line_info) {
  if (trailing_whitespace_ == WhitespaceState::kLeading ||
      trailing_whitespace_ == WhitespaceState::kNone ||
      trailing_whitespace_ == WhitespaceState::kCollapsed ||
      trailing_whitespace_ == WhitespaceState::kPreserved) {
    trailing_collapsible_space_.reset();
    return;
  }
  DCHECK(trailing_whitespace_ == WhitespaceState::kUnknown ||
         trailing_whitespace_ == WhitespaceState::kCollapsible);

  trailing_whitespace_ = WhitespaceState::kNone;
  const String& text = Text();
  NGInlineItemResults* item_results = line_info->MutableResults();
  for (auto it = item_results->rbegin(); it != item_results->rend(); ++it) {
    NGInlineItemResult& item_result = *it;
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item.EndCollapseType() == NGInlineItem::kOpaqueToCollapsing)
      continue;
    if (item.Type() == NGInlineItem::kText) {
      DCHECK_GT(item_result.EndOffset(), 0u);
      DCHECK(item.Style());
      if (!IsBreakableSpace(text[item_result.EndOffset() - 1]))
        break;
      if (!item.Style()->CollapseWhiteSpace()) {
        trailing_whitespace_ = WhitespaceState::kPreserved;
        break;
      }
      // |shape_result| is nullptr if this is an overflow because BreakText()
      // uses kNoResultIfOverflow option.
      if (!item_result.shape_result)
        break;

      if (!trailing_collapsible_space_.has_value() ||
          trailing_collapsible_space_->item_result != &item_result) {
        trailing_collapsible_space_.emplace();
        trailing_collapsible_space_->item_result = &item_result;
        if (item_result.EndOffset() - 1 > item_result.StartOffset()) {
          trailing_collapsible_space_->collapsed_shape_result =
              TruncateLineEndResult(*line_info, item_result,
                                    item_result.EndOffset() - 1);
        }
      }
      trailing_whitespace_ = WhitespaceState::kCollapsible;
      return;
    }
    if (item.Type() == NGInlineItem::kControl) {
      if (item.TextType() == NGTextType::kForcedLineBreak) {
        DCHECK_EQ(text[item.StartOffset()], kNewlineCharacter);
        continue;
      }
      trailing_whitespace_ = WhitespaceState::kPreserved;
      trailing_collapsible_space_.reset();
      return;
    }
    break;
  }

  trailing_collapsible_space_.reset();
}

// Measure control items; new lines and tab, that are similar to text, affect
// layout, but do not need shaping/painting.
void NGLineBreaker::HandleControlItem(const NGInlineItem& item,
                                      NGLineInfo* line_info) {
  DCHECK_GE(item.Length(), 1u);
  if (item.TextType() == NGTextType::kForcedLineBreak) {
    DCHECK_EQ(Text()[item.StartOffset()], kNewlineCharacter);

    // Check overflow, because the last item may have overflowed.
    if (HandleOverflowIfNeeded(line_info))
      return;

    NGInlineItemResult* item_result = AddItem(item, line_info);
    item_result->should_create_line_box = true;
    item_result->has_only_trailing_spaces = true;
    MoveToNextOf(item);

    // Include following close tags. The difference is visible when they have
    // margin/border/padding.
    //
    // This is not a defined behavior, but legacy/WebKit do this for preserved
    // newlines and <br>s. Gecko does this only for preserved newlines (but
    // not for <br>s).
    const Vector<NGInlineItem>& items = Items();
    while (item_index_ < items.size()) {
      const NGInlineItem& next_item = items[item_index_];
      if (next_item.Type() == NGInlineItem::kCloseTag) {
        HandleCloseTag(next_item, line_info);
        continue;
      }
      break;
    }

    if (UNLIKELY(HasHyphen()))
      position_ -= RemoveHyphen(line_info->MutableResults());
    is_after_forced_break_ = true;
    line_info->SetIsLastLine(true);
    state_ = LineBreakState::kDone;
    return;
  }

  DCHECK_EQ(item.TextType(), NGTextType::kFlowControl);
  UChar character = Text()[item.StartOffset()];
  switch (character) {
    case kTabulationCharacter: {
      DCHECK(item.Style());
      const ComputedStyle& style = *item.Style();
      scoped_refptr<const ShapeResult> shape_result =
          ShapeResult::CreateForTabulationCharacters(
              &style.GetFont(), item.Direction(), style.GetTabSize(), position_,
              item.StartOffset(), item.Length());
      HandleText(item, *shape_result, line_info);
      return;
    }
    case kZeroWidthSpaceCharacter: {
      // <wbr> tag creates break opportunities regardless of auto_wrap.
      NGInlineItemResult* item_result = AddItem(item, line_info);
      // A generated break opportunity doesn't generate fragments, but we still
      // need to add this for rewind to find this opportunity. This will be
      // discarded in |NGInlineLayoutAlgorithm| when it generates fragments.
      if (!item.IsGeneratedForLineBreak())
        item_result->should_create_line_box = true;
      item_result->can_break_after = true;
      break;
    }
    case kCarriageReturnCharacter:
    case kFormFeedCharacter:
      // Ignore carriage return and form feed.
      // https://drafts.csswg.org/css-text-3/#white-space-processing
      // https://github.com/w3c/csswg-drafts/issues/855
      HandleEmptyText(item, line_info);
      return;
    default:
      NOTREACHED();
      HandleEmptyText(item, line_info);
      return;
  }
  MoveToNextOf(item);
}

void NGLineBreaker::HandleBidiControlItem(const NGInlineItem& item,
                                          NGLineInfo* line_info) {
  DCHECK_EQ(item.Length(), 1u);

  // Bidi control characters have enter/exit semantics. Handle "enter"
  // characters simialr to open-tag, while "exit" (pop) characters similar to
  // close-tag.
  UChar character = Text()[item.StartOffset()];
  bool is_pop = character == kPopDirectionalIsolateCharacter ||
                character == kPopDirectionalFormattingCharacter;
  NGInlineItemResults* item_results = line_info->MutableResults();
  if (is_pop) {
    if (!item_results->IsEmpty()) {
      NGInlineItemResult* item_result = AddItem(item, line_info);
      NGInlineItemResult* last = &(*item_results)[item_results->size() - 2];
      // Honor the last |can_break_after| if it's true, in case it was
      // artificially set to true for break-after-space.
      if (last->can_break_after) {
        item_result->can_break_after = last->can_break_after;
        last->can_break_after = false;
      } else {
        // Otherwise compute from the text. |LazyLineBreakIterator| knows how to
        // break around bidi control characters.
        ComputeCanBreakAfter(item_result, auto_wrap_, break_iterator_);
      }
    } else {
      AddItem(item, line_info);
    }
  } else {
    if (state_ == LineBreakState::kTrailing &&
        CanBreakAfterLast(*item_results)) {
      DCHECK(!line_info->IsLastLine());
      MoveToNextOf(item);
      state_ = LineBreakState::kDone;
      return;
    }
    NGInlineItemResult* item_result = AddItem(item, line_info);
    DCHECK(!item_result->can_break_after);
  }
  MoveToNextOf(item);
}

void NGLineBreaker::HandleAtomicInline(
    const NGInlineItem& item,
    LayoutUnit percentage_resolution_block_size_for_min_max,
    NGLineInfo* line_info) {
  DCHECK_EQ(item.Type(), NGInlineItem::kAtomicInline);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();

  const LayoutUnit remaining_width = RemainingAvailableWidth();
  bool ignore_overflow_if_negative_margin = false;
  if (state_ == LineBreakState::kContinue && remaining_width < 0) {
    const unsigned item_index = item_index_;
    DCHECK_EQ(item_index, static_cast<unsigned>(&item - Items().begin()));
    DCHECK(!line_info->HasOverflow());
    HandleOverflow(line_info);
    if (!line_info->HasOverflow() || item_index != item_index_)
      return;
    // Compute margins if this line overflows. Negative margins can put the
    // position back.
    DCHECK_NE(state_, LineBreakState::kContinue);
    ignore_overflow_if_negative_margin = true;
  }

  // Compute margins before computing overflow, because even when the current
  // position is beyond the end, negative margins can bring this item back to on
  // the current line.
  NGInlineItemResult* item_result = AddItem(item, line_info);
  item_result->margins =
      ComputeLineMarginsForVisualContainer(constraint_space_, style);
  LayoutUnit inline_margins = item_result->margins.InlineSum();
  if (UNLIKELY(ignore_overflow_if_negative_margin)) {
    DCHECK_LT(remaining_width, 0);
    // The margin isn't negative, or the negative margin isn't large enough to
    // put the position back. Break this line before this item.
    if (inline_margins >= remaining_width) {
      RemoveLastItem(line_info);
      return;
    }
    // This line once overflowed, but the negative margin puts the position
    // back.
    state_ = LineBreakState::kContinue;
    line_info->SetHasOverflow(false);
  }

  // Last item may have ended with a hyphen, because at that point the line may
  // have ended there. Remove it because there are more items.
  if (UNLIKELY(HasHyphen()))
    position_ -= RemoveHyphen(line_info->MutableResults());

  // When we're just computing min/max content sizes, we can skip the full
  // layout and just compute those sizes. On the other hand, for regular
  // layout we need to do the full layout and get the layout result.
  // Doing a full layout for min/max content can also have undesirable
  // side effects when that falls back to legacy layout.
  if (mode_ == NGLineBreakerMode::kContent) {
    item_result->layout_result =
        NGBlockNode(ToLayoutBox(item.GetLayoutObject()))
            .LayoutAtomicInline(constraint_space_, node_.Style(),
                                line_info->UseFirstLineStyle());
    item_result->inline_size =
        NGFragment(constraint_space_.GetWritingDirection(),
                   item_result->layout_result->PhysicalFragment())
            .InlineSize();
    item_result->inline_size += inline_margins;
  } else if (mode_ == NGLineBreakerMode::kMaxContent && max_size_cache_) {
    unsigned item_index = &item - Items().begin();
    item_result->inline_size = (*max_size_cache_)[item_index];
  } else {
    DCHECK(mode_ == NGLineBreakerMode::kMinContent || !max_size_cache_);
    NGBlockNode child(ToLayoutBox(item.GetLayoutObject()));
    MinMaxSizesInput input(percentage_resolution_block_size_for_min_max,
                           MinMaxSizesType::kContent);
    MinMaxSizesResult result =
        ComputeMinAndMaxContentContribution(node_.Style(), child, input);
    if (mode_ == NGLineBreakerMode::kMinContent) {
      item_result->inline_size = result.sizes.min_size + inline_margins;
      if (depends_on_percentage_block_size_out_) {
        *depends_on_percentage_block_size_out_ |=
            result.depends_on_percentage_block_size;
      }
      if (max_size_cache_) {
        if (max_size_cache_->IsEmpty())
          max_size_cache_->resize(Items().size());
        unsigned item_index = &item - Items().begin();
        (*max_size_cache_)[item_index] = result.sizes.max_size + inline_margins;
      }
    } else {
      item_result->inline_size = result.sizes.max_size + inline_margins;
    }
  }

  item_result->should_create_line_box = true;
  // Atomic inlines have break opportunities before and after, even when the
  // adjacent character is U+00A0 NO-BREAK SPACE character, except when sticky
  // images quirk is applied.
  item_result->can_break_after =
      auto_wrap_ && !(sticky_images_quirk_ && item.IsImage());

  position_ += item_result->inline_size;

  if (item.IsRubyRun()) {
    // Overrides can_break_after.
    ComputeCanBreakAfter(item_result, auto_wrap_, break_iterator_);

    NGAnnotationOverhang overhang = GetOverhang(*item_result);
    if (overhang.end > LayoutUnit()) {
      item_result->pending_end_overhang = overhang.end;
      maybe_have_end_overhang_ = true;
    }

    if (CanApplyStartOverhang(*line_info, overhang.start)) {
      DCHECK_EQ(item_result->margins.inline_start, LayoutUnit());
      item_result->margins.inline_start = -overhang.start;
      item_result->inline_size -= overhang.start;
      position_ -= overhang.start;
    }
  }

  trailing_whitespace_ = WhitespaceState::kNone;
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
void NGLineBreaker::HandleFloat(const NGInlineItem& item,
                                NGLineInfo* line_info) {
  // When rewind occurs, an item may be handled multiple times.
  // Since floats are put into a separate list, avoid handling same floats
  // twice.
  // Ideally rewind can take floats out of floats list, but the difference is
  // sutble compared to the complexity.
  //
  // Additionally, we need to skip floats if we're retrying a line after a
  // fragmentainer break. In that case the floats associated with this line will
  // already have been processed.
  NGInlineItemResult* item_result = AddItem(item, line_info);
  item_result->can_break_after = auto_wrap_;
  MoveToNextOf(item);

  // If we are currently computing our min/max-content size, simply append the
  // unpositioned floats to |NGLineInfo| and abort.
  if (mode_ != NGLineBreakerMode::kContent)
    return;

  // Make sure we populate the positioned_float inside the |item_result|.
  if (item_index_ <= handled_leading_floats_index_ &&
      !leading_floats_.IsEmpty()) {
    DCHECK_LT(leading_floats_index_, leading_floats_.size());
    item_result->positioned_float = leading_floats_[leading_floats_index_++];

    // Don't break after leading floats if indented.
    if (position_ != 0)
      item_result->can_break_after = false;
    return;
  }

  LayoutUnit bfc_block_offset = line_opportunity_.bfc_block_offset;
  NGUnpositionedFloat unpositioned_float(
      NGBlockNode(ToLayoutBox(item.GetLayoutObject())),
      /* break_token */ nullptr, constraint_space_.AvailableSize(),
      constraint_space_.PercentageResolutionSize(),
      constraint_space_.ReplacedPercentageResolutionSize(),
      {constraint_space_.BfcOffset().line_offset, bfc_block_offset},
      constraint_space_, node_.Style());

  LayoutUnit inline_margin_size =
      ComputeMarginBoxInlineSizeForUnpositionedFloat(&unpositioned_float);

  LayoutUnit used_size = position_ + inline_margin_size +
                         ComputeFloatAncestorInlineEndSize(
                             constraint_space_, Items(), item_index_);
  bool can_fit_float =
      used_size <= line_opportunity_.AvailableFloatInlineSize().AddEpsilon();
  if (!can_fit_float) {
    // Floats need to know the current line width to determine whether to put it
    // into the current line or to the next line. Trailing spaces will be
    // removed if this line breaks here because they should be collapsed across
    // floats, but they are still included in the current line position at this
    // point. Exclude it when computing whether this float can fit or not.
    can_fit_float = used_size - TrailingCollapsibleSpaceWidth(line_info) <=
                    line_opportunity_.AvailableFloatInlineSize().AddEpsilon();
  }

  // The float should be positioned after the current line if:
  //  - It can't fit within the non-shape area. (Assuming the current position
  //    also is strictly within the non-shape area).
  //  - It will be moved down due to block-start edge alignment.
  //  - It will be moved down due to clearance.
  //  - An earlier float has been pushed to the next fragmentainer.
  bool float_after_line =
      !can_fit_float ||
      exclusion_space_->LastFloatBlockStart() > bfc_block_offset ||
      exclusion_space_->ClearanceOffset(unpositioned_float.ClearType(
          constraint_space_.Direction())) > bfc_block_offset;

  // Check if we already have a pending float. That's because a float cannot be
  // higher than any block or floated box generated before.
  if (HasUnpositionedFloats(line_info->Results()) || float_after_line) {
    item_result->has_unpositioned_floats = true;
    return;
  }

  NGPositionedFloat positioned_float =
      PositionFloat(&unpositioned_float, exclusion_space_);

  if (constraint_space_.HasBlockFragmentation()) {
    if (positioned_float.need_break_before) {
      // We broke before the float, and there's no fragment. Create a break
      // token and propagate it all the way to the block container layout
      // algorithm. The float will start in the next fragmentainer.
      auto break_before = NGBlockBreakToken::CreateBreakBefore(
          unpositioned_float.node, /* is_forced_break */ false);
      RemoveLastItem(line_info);
      PropagateBreakToken(break_before);
      return;
    }
    // If we broke inside the float, we also need to propagate a break token to
    // the block container. Layout of the float will resume in the next
    // fragmentainer.
    if (scoped_refptr<const NGBreakToken> token =
            positioned_float.layout_result->PhysicalFragment().BreakToken())
      PropagateBreakToken(std::move(To<NGBlockBreakToken>(token.get())));
  }

  item_result->positioned_float = positioned_float;

  NGLayoutOpportunity opportunity = exclusion_space_->FindLayoutOpportunity(
      {constraint_space_.BfcOffset().line_offset, bfc_block_offset},
      constraint_space_.AvailableSize().inline_size);

  DCHECK_EQ(bfc_block_offset, opportunity.rect.BlockStartOffset());

  line_opportunity_ = opportunity.ComputeLineLayoutOpportunity(
      constraint_space_, line_opportunity_.line_block_size, LayoutUnit());
  available_width_ = ComputeAvailableWidth();

  DCHECK_GE(AvailableWidth(), LayoutUnit());
}

void NGLineBreaker::HandleOutOfFlowPositioned(const NGInlineItem& item,
                                              NGLineInfo* line_info) {
  DCHECK_EQ(item.Type(), NGInlineItem::kOutOfFlowPositioned);
  NGInlineItemResult* item_result = AddItem(item, line_info);

  // Break opportunity after OOF is not well-defined nor interoperable. Using
  // |kObjectReplacementCharacter|, except when this is a leading OOF, seems to
  // produce reasonable and interoperable results in common cases.
  DCHECK(!item_result->can_break_after);
  if (item_result->should_create_line_box)
    ComputeCanBreakAfter(item_result, auto_wrap_, break_iterator_);

  MoveToNextOf(item);
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
      (style.HasBorder() || style.MayHavePadding() ||
       (style.MayHaveMargin() && item_result->has_edge))) {
    item_result->borders = ComputeLineBorders(style);
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

void NGLineBreaker::HandleOpenTag(const NGInlineItem& item,
                                  NGLineInfo* line_info) {
  DCHECK_EQ(item.Type(), NGInlineItem::kOpenTag);

  NGInlineItemResult* item_result = AddItem(item, line_info);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  if (ComputeOpenTagResult(item, constraint_space_, item_result)) {
    // Negative margins on open tags may bring the position back. Update
    // |state_| if that happens.
    if (UNLIKELY(item_result->inline_size < 0 &&
                 state_ == LineBreakState::kTrailing)) {
      LayoutUnit available_width = AvailableWidthToFit();
      if (position_ > available_width &&
          position_ + item_result->inline_size <= available_width) {
        state_ = LineBreakState::kContinue;
      }
    }

    position_ += item_result->inline_size;

    // While the spec defines "non-zero margins, padding, or borders" prevents
    // line boxes to be zero-height, tests indicate that only inline direction
    // of them do so. See should_create_line_box_.
    // Force to create a box, because such inline boxes affect line heights.
    if (!item_result->should_create_line_box && !item.IsEmptyItem())
      item_result->should_create_line_box = true;

    if (UNLIKELY(style.BoxDecorationBreak() == EBoxDecorationBreak::kClone)) {
      has_cloned_box_decorations_ = true;
      ++cloned_box_decorations_count_;
      cloned_box_decorations_end_size_ += item_result->margins.inline_end +
                                          item_result->borders.inline_end +
                                          item_result->padding.inline_end;
    }
  }

  bool was_auto_wrap = auto_wrap_;
  SetCurrentStyle(style);
  MoveToNextOf(item);

  DCHECK(!item_result->can_break_after);
  const NGInlineItemResults& item_results = line_info->Results();
  if (UNLIKELY(!was_auto_wrap && auto_wrap_ && item_results.size() >= 2)) {
    if (IsPreviousItemOfType(NGInlineItem::kText))
      ComputeCanBreakAfter(std::prev(item_result), auto_wrap_, break_iterator_);
  }
}

void NGLineBreaker::HandleCloseTag(const NGInlineItem& item,
                                   NGLineInfo* line_info) {
  NGInlineItemResult* item_result = AddItem(item, line_info);

  item_result->has_edge = item.HasEndEdge();
  if (item_result->has_edge) {
    DCHECK(item.Style());
    const ComputedStyle& style = *item.Style();
    item_result->inline_size = ComputeInlineEndSize(constraint_space_, &style);
    position_ += item_result->inline_size;

    if (!item_result->should_create_line_box && !item.IsEmptyItem())
      item_result->should_create_line_box = true;

    if (UNLIKELY(style.BoxDecorationBreak() == EBoxDecorationBreak::kClone)) {
      DCHECK_GT(cloned_box_decorations_count_, 0u);
      --cloned_box_decorations_count_;
      DCHECK_GE(cloned_box_decorations_end_size_, item_result->inline_size);
      cloned_box_decorations_end_size_ -= item_result->inline_size;
    }
  }
  DCHECK(item.GetLayoutObject() && item.GetLayoutObject()->Parent());
  bool was_auto_wrap = auto_wrap_;
  SetCurrentStyle(item.GetLayoutObject()->Parent()->StyleRef());
  MoveToNextOf(item);

  // If the line can break after the previous item, prohibit it and allow break
  // after this close tag instead. Even when the close tag has "nowrap", break
  // after it is allowed if the line is breakable after the previous item.
  const NGInlineItemResults& item_results = line_info->Results();
  if (item_results.size() >= 2) {
    NGInlineItemResult* last = std::prev(item_result);
    if (was_auto_wrap || last->can_break_after) {
      item_result->can_break_after =
          last->can_break_after ||
          IsBreakableSpace(Text()[item_result->EndOffset()]);
      last->can_break_after = false;
      return;
    }
    if (auto_wrap_ && !IsBreakableSpace(Text()[item_result->EndOffset() - 1]))
      ComputeCanBreakAfter(item_result, auto_wrap_, break_iterator_);
  }
}

// Handles when the last item overflows.
// At this point, item_results does not fit into the current line, and there
// are no break opportunities in item_results.back().
void NGLineBreaker::HandleOverflow(NGLineInfo* line_info) {
  // Compute the width needing to rewind. When |width_to_rewind| goes negative,
  // items can fit within the line.
  LayoutUnit available_width = AvailableWidthToFit();
  LayoutUnit width_to_rewind = position_ - available_width;
  DCHECK_GT(width_to_rewind, 0);

  // Keep track of the shortest break opportunity.
  unsigned break_before = 0;

  // True if there is at least one item that has `break-word`.
  bool has_break_anywhere_if_overflow = break_anywhere_if_overflow_;

  // Save the hyphenation states before we may make changes.
  NGInlineItemResults* item_results = line_info->MutableResults();
  base::Optional<wtf_size_t> hyphen_index_before = hyphen_index_;
  if (UNLIKELY(HasHyphen()))
    position_ -= RemoveHyphen(item_results);

  // Search for a break opportunity that can fit.
  for (unsigned i = item_results->size(); i;) {
    NGInlineItemResult* item_result = &(*item_results)[--i];
    has_break_anywhere_if_overflow |= item_result->break_anywhere_if_overflow;

    // Try to break after this item.
    if (i < item_results->size() - 1 && item_result->can_break_after) {
      if (width_to_rewind <= 0) {
        position_ = available_width + width_to_rewind;
        RewindOverflow(i + 1, line_info);
        return;
      }
      break_before = i + 1;
    }

    // Compute the position after this item was removed entirely.
    width_to_rewind -= item_result->inline_size;

    // Try next if still does not fit.
    if (width_to_rewind > 0)
      continue;

    DCHECK(item_result->item);
    const NGInlineItem& item = *item_result->item;
    if (item.Type() == NGInlineItem::kText) {
      DCHECK(item_result->shape_result ||
             (item_result->break_anywhere_if_overflow &&
              !override_break_anywhere_) ||
             // |HandleTextForFastMinContent| can produce an item without
             // |ShapeResult|. In this case, it is not breakable.
             (mode_ == NGLineBreakerMode::kMinContent &&
              !item_result->may_break_inside));
      // If space is available, and if this text is breakable, part of the text
      // may fit. Try to break this item.
      if (width_to_rewind < 0 && item_result->may_break_inside) {
        const LayoutUnit item_available_width = -width_to_rewind;
        // Make sure the available width is smaller than the current width. The
        // break point must not be at the end when e.g., the text fits but its
        // right margin does not or following items do not.
        const LayoutUnit min_available_width = item_result->inline_size - 1;
        // If |inline_size| is zero (e.g., `font-size: 0`), |BreakText| cannot
        // make it shorter. Take the previous break opportunity.
        if (UNLIKELY(min_available_width <= 0)) {
          if (BreakTextAtPreviousBreakOpportunity(item_result)) {
            RewindOverflow(i + 1, line_info);
            return;
          }
          continue;
        }
        scoped_refptr<const ComputedStyle> was_current_style = current_style_;
        SetCurrentStyle(*item.Style());
        const NGInlineItemResult item_result_before = *item_result;
        BreakText(item_result, item, *item.TextShapeResult(),
                  std::min(item_available_width, min_available_width),
                  item_available_width, line_info);
        DCHECK_LE(item_result->EndOffset(), item_result_before.EndOffset());
#if DCHECK_IS_ON()
        item_result->CheckConsistency(true);
#endif

        // If BreakText() changed this item small enough to fit, break here.
        if (item_result->can_break_after &&
            item_result->inline_size <= item_available_width &&
            item_result->EndOffset() < item_result_before.EndOffset()) {
          DCHECK_LT(item_result->EndOffset(), item.EndOffset());

          // If this is the last item, adjust it to accommodate the change.
          const unsigned new_end = i + 1;
          CHECK_LE(new_end, item_results->size());
          if (new_end == item_results->size()) {
            position_ =
                available_width + width_to_rewind + item_result->inline_size;
            DCHECK_EQ(position_, line_info->ComputeWidth());
            item_index_ = item_result->item_index;
            offset_ = item_result->EndOffset();
            items_data_.AssertOffset(item_index_, offset_);
            HandleTrailingSpaces(item, line_info);
            return;
          }

          state_ = LineBreakState::kTrailing;
          Rewind(new_end, line_info);
          return;
        }

        // Failed to break to fit. Restore to the original state.
        if (UNLIKELY(HasHyphen()))
          RemoveHyphen(item_results);
        *item_result = std::move(item_result_before);
        SetCurrentStyle(*was_current_style);
      }
    }
  }

  // Reaching here means that the rewind point was not found.

  if (!override_break_anywhere_ && has_break_anywhere_if_overflow) {
    override_break_anywhere_ = true;
    break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
    state_ = LineBreakState::kContinue;
    // TODO(kojii): Not all items need to rewind, but such case is rare and
    // rewinding all items simplifes the code.
    if (!item_results->IsEmpty())
      Rewind(0, line_info);
    ResetRewindLoopDetector();
    return;
  }

  // Let this line overflow.
  line_info->SetHasOverflow();

  // Restore the hyphenation states to before the loop if needed.
  DCHECK(!HasHyphen());
  if (UNLIKELY(hyphen_index_before))
    position_ += AddHyphen(item_results, *hyphen_index_before);

  // If there was a break opportunity, the overflow should stop there.
  if (break_before) {
    RewindOverflow(break_before, line_info);
    return;
  }

  if (CanBreakAfterLast(*item_results)) {
    state_ = LineBreakState::kTrailing;
    return;
  }

  // No break opportunities. Break at the earliest break opportunity.
  DCHECK(std::all_of(item_results->begin(), item_results->end(),
                     [](const NGInlineItemResult& item_result) {
                       return !item_result.can_break_after;
                     }));
  state_ = LineBreakState::kOverflow;
}

// Rewind to |new_end| on overflow. If trailable items follow at |new_end|, they
// are included (not rewound).
void NGLineBreaker::RewindOverflow(unsigned new_end, NGLineInfo* line_info) {
  const Vector<NGInlineItem>& items = Items();
  const NGInlineItemResults& item_results = line_info->Results();
  DCHECK_LT(new_end, item_results.size());

  unsigned open_tag_count = 0;
  const String& text = Text();
  for (unsigned index = new_end; index < item_results.size(); index++) {
    const NGInlineItemResult& item_result = item_results[index];
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item.Type() == NGInlineItem::kText) {
      // Text items are trailable if they start with trailable spaces.
      DCHECK_GT(item_result.Length(), 0u);
      if (item_result.shape_result ||  // kNoResultIfOverflow if 'break-word'
          (break_anywhere_if_overflow_ && !override_break_anywhere_)) {
        DCHECK(item.Style());
        const EWhiteSpace white_space = item.Style()->WhiteSpace();
        if (ComputedStyle::AutoWrap(white_space) &&
            white_space != EWhiteSpace::kBreakSpaces &&
            IsBreakableSpace(text[item_result.StartOffset()])) {
          // If all characters are trailable spaces, check the next item.
          if (item_result.shape_result &&
              IsAllBreakableSpaces(text, item_result.StartOffset() + 1,
                                   item_result.EndOffset())) {
            continue;
          }
          // If this item starts with spaces followed by non-space characters,
          // the line should break after the spaces. Rewind to before this item.
          //
          // After the rewind, we want to |HandleTrailingSpaces| in this |item|,
          // but |Rewind| may have failed when we have floats. Set the |state_|
          // to |kTrailing| and let the next |HandleText| to handle this.
          state_ = LineBreakState::kTrailing;
          Rewind(index, line_info);
          return;
        }
      }
    } else if (item.Type() == NGInlineItem::kControl) {
      // All control characters except newline are trailable if auto_wrap. We
      // should not have rewound if there was a newline, so safe to assume all
      // controls are trailable.
      DCHECK_NE(text[item_result.StartOffset()], kNewlineCharacter);
      DCHECK(item.Style());
      EWhiteSpace white_space = item.Style()->WhiteSpace();
      if (ComputedStyle::AutoWrap(white_space) &&
          white_space != EWhiteSpace::kBreakSpaces)
        continue;
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      // Open tags are ambiguous. This open tag is not trailable:
      //   <span>text
      // but these are trailable:
      //   <span> text
      //   <span></span>text
      //   <span> </span>text
      // Count the nest-level and mark where the nest-level was 0.
      if (!open_tag_count)
        new_end = index;
      open_tag_count++;
      continue;
    } else if (item.Type() == NGInlineItem::kCloseTag) {
      if (open_tag_count > 0)
        open_tag_count--;
      continue;
    } else if (IsTrailableItemType(item.Type())) {
      continue;
    }

    // Found a non-trailable item. Rewind to before the item, or to before the
    // open tag if the nest-level is not zero.
    if (open_tag_count)
      index = new_end;
    state_ = LineBreakState::kDone;
    DCHECK(!line_info->IsLastLine());
    Rewind(index, line_info);
    return;
  }

  // The open tag turned out to be non-trailable if the nest-level is not zero.
  // Rewind to before the open tag.
  if (open_tag_count) {
    state_ = LineBreakState::kDone;
    DCHECK(!line_info->IsLastLine());
    Rewind(new_end, line_info);
    return;
  }

  // All items are trailable. Done without rewinding.
  trailing_whitespace_ = WhitespaceState::kUnknown;
  position_ = line_info->ComputeWidth();
  state_ = LineBreakState::kDone;
  DCHECK(!line_info->IsLastLine());
  if (item_index_ == items.size())
    line_info->SetIsLastLine(true);
}

void NGLineBreaker::Rewind(unsigned new_end, NGLineInfo* line_info) {
  NGInlineItemResults& item_results = *line_info->MutableResults();
  DCHECK_LT(new_end, item_results.size());
  if (last_rewind_) {
    // Detect rewind-loop. If we're trying to rewind to the same index twice,
    // we're in the infinite loop.
    if (item_index_ == last_rewind_->from_item_index &&
        new_end == last_rewind_->to_index) {
      NOTREACHED();
      state_ = LineBreakState::kDone;
      return;
    }
    last_rewind_.emplace(RewindIndex{item_index_, new_end});
  }

  // Avoid rewinding floats if possible. They will be added back anyway while
  // processing trailing items even when zero available width. Also this saves
  // most cases where our support for rewinding positioned floats is not great
  // yet (see below.)
  while (item_results[new_end].item->Type() == NGInlineItem::kFloating) {
    // We assume floats can break after, or this may cause an infinite loop.
    DCHECK(item_results[new_end].can_break_after);
    ++new_end;
    if (new_end == item_results.size()) {
      if (UNLIKELY(!hyphen_index_ && has_any_hyphens_))
        RestoreLastHyphen(&item_results);
      position_ = line_info->ComputeWidth();
      return;
    }
  }

  // Because floats are added to |positioned_floats_| or |unpositioned_floats_|,
  // rewinding them needs to remove from these lists too.
  for (unsigned i = item_results.size(); i > new_end;) {
    NGInlineItemResult& rewind = item_results[--i];
    if (rewind.positioned_float) {
      // We assume floats can break after, or this may cause an infinite loop.
      DCHECK(rewind.can_break_after);
      // TODO(kojii): We do not have mechanism to remove once positioned floats
      // yet, and that rewinding them may lay it out twice. For now, prohibit
      // rewinding positioned floats. This may results in incorrect layout, but
      // still better than rewinding them.
      new_end = i + 1;
      if (new_end == item_results.size()) {
        if (UNLIKELY(!hyphen_index_ && has_any_hyphens_))
          RestoreLastHyphen(&item_results);
        position_ = line_info->ComputeWidth();
        return;
      }
      break;
    }
  }

  if (new_end) {
    // Use |results[new_end - 1].end_offset| because it may have been truncated
    // and may not be equal to |results[new_end].start_offset|.
    MoveToNextOf(item_results[new_end - 1]);
    trailing_whitespace_ = WhitespaceState::kUnknown;
    // When space item is followed by empty text, we will break line at empty
    // text. See http://crbug.com/1104534
    // Example:
    //   [0] kOpeNTag 0-0 <i>
    //   [1] kText 0-10 "012345679"
    //   [2] kOpenTag 10-10 <b> <= |item_results[new_end - 1]|
    //   [3] kText 10-10 ""     <= |item_index_|
    //   [4] kText 10-11 " "
    //   [5] kCloseTag 11-11 <b>
    //   [6] kText 11-13 "ab"
    //   [7] kCloseTag 13-13 <i>
    // Note: We can have multiple empty |LayoutText| by ::first-letter, nested
    // <q>, Text.splitText(), etc.
    const Vector<NGInlineItem>& items = Items();
    while (item_index_ < items.size() &&
           items[item_index_].Type() == NGInlineItem::kText &&
           !items[item_index_].Length())
      HandleEmptyText(items[item_index_], line_info);
  } else {
    // When rewinding all items, use |results[0].start_offset|.
    const NGInlineItemResult& first_remove = item_results[new_end];
    item_index_ = first_remove.item_index;
    offset_ = first_remove.StartOffset();
    trailing_whitespace_ = WhitespaceState::kLeading;
    maybe_have_end_overhang_ = false;
  }
  SetCurrentStyle(ComputeCurrentStyle(new_end, line_info));

  item_results.Shrink(new_end);

  trailing_collapsible_space_.reset();
  if (UNLIKELY(hyphen_index_ && *hyphen_index_ >= new_end))
    hyphen_index_.reset();
  if (UNLIKELY(!hyphen_index_ && has_any_hyphens_))
    RestoreLastHyphen(&item_results);
  position_ = line_info->ComputeWidth();
  if (UNLIKELY(has_cloned_box_decorations_))
    RecalcClonedBoxDecorations();
}

// Returns the style to use for |item_result_index|. Normally when handling
// items sequentially, the current style is updated on open/close tag. When
// rewinding, this function computes the style for the specified item.
const ComputedStyle& NGLineBreaker::ComputeCurrentStyle(
    unsigned item_result_index,
    NGLineInfo* line_info) const {
  const NGInlineItemResults& item_results = line_info->Results();

  // Use the current item if it can compute the current style.
  const NGInlineItem* item = item_results[item_result_index].item;
  DCHECK(item);
  if (item->Type() == NGInlineItem::kText ||
      item->Type() == NGInlineItem::kCloseTag) {
    DCHECK(item->Style());
    return *item->Style();
  }

  // Otherwise look back an item that can compute the current style.
  while (item_result_index) {
    item = item_results[--item_result_index].item;
    if (item->Type() == NGInlineItem::kText ||
        item->Type() == NGInlineItem::kOpenTag) {
      DCHECK(item->Style());
      return *item->Style();
    }
    if (item->Type() == NGInlineItem::kCloseTag)
      return item->GetLayoutObject()->Parent()->StyleRef();
  }

  // Use the style at the beginning of the line if no items are available.
  if (break_token_) {
    DCHECK(break_token_->Style());
    return *break_token_->Style();
  }
  return line_info->LineStyle();
}

void NGLineBreaker::SetCurrentStyle(const ComputedStyle& style) {
  if (&style == current_style_.get()) {
#if DCHECK_IS_ON()
    // Check that cache fields are already setup correctly.
    DCHECK_EQ(auto_wrap_, style.AutoWrap());
    if (auto_wrap_) {
      DCHECK_EQ(enable_soft_hyphen_, style.GetHyphens() != Hyphens::kNone);
      DCHECK_EQ(break_iterator_.Locale(), style.LocaleForLineBreakIterator());
    }
    ShapeResultSpacing<String> spacing(spacing_.Text());
    spacing.SetSpacing(style.GetFont());
    DCHECK_EQ(spacing.LetterSpacing(), spacing_.LetterSpacing());
    DCHECK_EQ(spacing.WordSpacing(), spacing_.WordSpacing());
#endif
    return;
  }
  current_style_ = &style;

  auto_wrap_ = style.AutoWrap();

  if (auto_wrap_) {
    LineBreakType line_break_type;
    EWordBreak word_break = style.WordBreak();
    switch (word_break) {
      case EWordBreak::kNormal:
        line_break_type = LineBreakType::kNormal;
        break;
      case EWordBreak::kBreakAll:
        line_break_type = LineBreakType::kBreakAll;
        break;
      case EWordBreak::kBreakWord:
        line_break_type = LineBreakType::kNormal;
        break;
      case EWordBreak::kKeepAll:
        line_break_type = LineBreakType::kKeepAll;
        break;
    }
    EOverflowWrap overflow_wrap = style.OverflowWrap();
    break_anywhere_if_overflow_ =
        word_break == EWordBreak::kBreakWord ||
        overflow_wrap == EOverflowWrap::kAnywhere ||
        // `overflow-/word-wrap: break-word` affects layout but not min-content.
        (overflow_wrap == EOverflowWrap::kBreakWord &&
         mode_ == NGLineBreakerMode::kContent);
    if (UNLIKELY((override_break_anywhere_ && break_anywhere_if_overflow_) ||
                 style.GetLineBreak() == LineBreak::kAnywhere)) {
      line_break_type = LineBreakType::kBreakCharacter;
    }
    break_iterator_.SetBreakType(line_break_type);

    enable_soft_hyphen_ = style.GetHyphens() != Hyphens::kNone;
    hyphenation_ = style.GetHyphenation();

    if (style.WhiteSpace() == EWhiteSpace::kBreakSpaces)
      break_iterator_.SetBreakSpace(BreakSpaceType::kAfterEverySpace);

    break_iterator_.SetLocale(style.LocaleForLineBreakIterator());
  }

  spacing_.SetSpacing(style.GetFont());
}

bool NGLineBreaker::IsPreviousItemOfType(NGInlineItem::NGInlineItemType type) {
  return item_index_ > 0 ? Items().at(item_index_ - 1).Type() == type : false;
}

void NGLineBreaker::MoveToNextOf(const NGInlineItem& item) {
  offset_ = item.EndOffset();
  item_index_++;
#if DCHECK_IS_ON()
  const Vector<NGInlineItem>& items = Items();
  if (item_index_ < items.size()) {
    items[item_index_].AssertOffset(offset_);
  } else {
    DCHECK_EQ(offset_, Text().length());
  }
#endif
}

void NGLineBreaker::MoveToNextOf(const NGInlineItemResult& item_result) {
  offset_ = item_result.EndOffset();
  item_index_ = item_result.item_index;
  DCHECK(item_result.item);
  if (offset_ == item_result.item->EndOffset())
    item_index_++;
}

scoped_refptr<NGInlineBreakToken> NGLineBreaker::CreateBreakToken(
    const NGLineInfo& line_info) const {
  DCHECK(current_style_);
  const Vector<NGInlineItem>& items = Items();
  DCHECK_LE(item_index_, items.size());
  if (item_index_ >= items.size())
    return NGInlineBreakToken::Create(node_);
  return NGInlineBreakToken::Create(
      node_, current_style_.get(), item_index_, offset_,
      (is_after_forced_break_ ? NGInlineBreakToken::kIsForcedBreak : 0) |
          (line_info.UseFirstLineStyle()
               ? NGInlineBreakToken::kUseFirstLineStyle
               : 0) |
          (cloned_box_decorations_count_
               ? NGInlineBreakToken::kHasClonedBoxDecorations
               : 0));
}

void NGLineBreaker::PropagateBreakToken(
    scoped_refptr<const NGBlockBreakToken> token) {
  propagated_break_tokens_.push_back(std::move(token));
}

}  // namespace blink
