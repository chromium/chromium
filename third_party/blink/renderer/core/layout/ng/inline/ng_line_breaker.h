// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BREAKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BREAKER_H_

#include "base/check_op.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_line_layout_opportunity.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Hyphenation;
class NGBlockBreakToken;
class NGColumnSpannerPath;
class NGInlineBreakToken;
class NGInlineItem;
class NGLineInfo;
class ResolvedTextLayoutAttributesIterator;

// The line breaker needs to know which mode its in to properly handle floats.
enum class NGLineBreakerMode { kContent, kMinContent, kMaxContent };

// Represents a line breaker.
//
// This class measures each NGInlineItem and determines items to form a line,
// so that NGInlineLayoutAlgorithm can build a line box from the output.
class CORE_EXPORT NGLineBreaker {
  STACK_ALLOCATED();

 public:
  NGLineBreaker(NGInlineNode,
                NGLineBreakerMode,
                const NGConstraintSpace&,
                const NGLineLayoutOpportunity&,
                const NGPositionedFloatVector& leading_floats,
                unsigned handled_leading_floats_index,
                const NGInlineBreakToken*,
                const NGColumnSpannerPath*,
                NGExclusionSpace*);
  ~NGLineBreaker();

  const NGInlineItemsData& ItemsData() const { return items_data_; }

  // True if the last line has `box-decoration-break: clone`, which affected the
  // size.
  bool HasClonedBoxDecorations() const { return has_cloned_box_decorations_; }

  // Compute the next line break point and produces NGInlineItemResults for
  // the line.
  void NextLine(NGLineInfo*);

  bool IsFinished() const { return item_index_ >= Items().size(); }

  void PropagateBreakToken(const NGBlockBreakToken*);
  HeapVector<Member<const NGBlockBreakToken>>& PropagatedBreakTokens() {
    return propagated_break_tokens_;
  }

  // Computing |NGLineBreakerMode::kMinContent| with |MaxSizeCache| caches
  // information that can help computing |kMaxContent|. It is recommended to set
  // this when computing both |kMinContent| and |kMaxContent|.
  using MaxSizeCache = Vector<LayoutUnit, 64>;
  void SetIntrinsicSizeOutputs(MaxSizeCache* max_size_cache,
                               bool* depends_on_block_constraints_out);

  // Compute NGInlineItemResult for an open tag item.
  // Returns true if this item has edge and may have non-zero inline size.
  static bool ComputeOpenTagResult(const NGInlineItem&,
                                   const NGConstraintSpace&,
                                   bool is_in_svg_text,
                                   NGInlineItemResult*);

  // This enum is private, except for |WhitespaceStateForTesting()|. See
  // |whitespace_| member.
  enum class WhitespaceState {
    kLeading,
    kNone,
    kUnknown,
    kCollapsible,
    kCollapsed,
    kPreserved,
  };
  WhitespaceState TrailingWhitespaceForTesting() const {
    return trailing_whitespace_;
  }

 private:
  const String& Text() const { return text_content_; }
  const HeapVector<NGInlineItem>& Items() const { return items_data_.items; }

  String TextContentForLineBreak() const;

  NGInlineItemResult* AddItem(const NGInlineItem&,
                              unsigned end_offset,
                              NGLineInfo*);
  NGInlineItemResult* AddItem(const NGInlineItem&, NGLineInfo*);

  void BreakLine(NGLineInfo*);
  void PrepareNextLine(NGLineInfo*);

  void ComputeLineLocation(NGLineInfo*) const;

  // Returns true if CSS property "white-space" specified in |style| allows
  // wrap. Note: For "text-combine-upright:all", this function returns false
  // event if "white-space" means wrap, because combined text should be laid
  // out in one line.
  bool ShouldAutoWrap(const ComputedStyle& style) const;

  enum class LineBreakState {
    // The line breaking is complete.
    kDone,

    // Overflow is detected without any earlier break opportunities. This line
    // should break at the earliest break opportunity.
    kOverflow,

    // Should complete the line at the earliest possible point.
    // Trailing spaces, <br>, or close tags should be included to the line even
    // when it is overflowing.
    kTrailing,

    // Looking for more items to fit into the current line.
    kContinue,
  };

  void HandleText(const NGInlineItem& item, const ShapeResult&, NGLineInfo*);
  // Split |item| into segments, and add them to |line_info|.
  // This is for SVG <text>.
  void SplitTextIntoSegments(const NGInlineItem& item, NGLineInfo* line_info);
  // Returns true if we should split NGInlineItem before
  // svg_addressable_offset_.
  bool ShouldCreateNewSvgSegment() const;
  enum BreakResult { kSuccess, kOverflow };
  BreakResult BreakText(NGInlineItemResult*,
                        const NGInlineItem&,
                        const ShapeResult&,
                        LayoutUnit available_width,
                        LayoutUnit available_width_with_hyphens,
                        NGLineInfo*);
  bool BreakTextAtPreviousBreakOpportunity(NGInlineItemResult* item_result);
  bool HandleTextForFastMinContent(NGInlineItemResult*,
                                   const NGInlineItem&,
                                   const ShapeResult&,
                                   NGLineInfo*);
  void HandleEmptyText(const NGInlineItem& item, NGLineInfo*);

  scoped_refptr<ShapeResultView> TruncateLineEndResult(
      const NGLineInfo&,
      const NGInlineItemResult&,
      unsigned end_offset);
  void UpdateShapeResult(const NGLineInfo&, NGInlineItemResult*);
  scoped_refptr<ShapeResult> ShapeText(const NGInlineItem&,
                                       unsigned start,
                                       unsigned end);

  void HandleTrailingSpaces(const NGInlineItem&, NGLineInfo*);
  void HandleTrailingSpaces(const NGInlineItem&,
                            const ShapeResult*,
                            NGLineInfo*);
  void RemoveTrailingCollapsibleSpace(NGLineInfo*);
  LayoutUnit TrailingCollapsibleSpaceWidth(NGLineInfo*);
  void ComputeTrailingCollapsibleSpace(NGLineInfo*);
  void RewindTrailingOpenTags(NGLineInfo*);

  void HandleControlItem(const NGInlineItem&, NGLineInfo*);
  void HandleForcedLineBreak(const NGInlineItem*, NGLineInfo*);
  void HandleBidiControlItem(const NGInlineItem&, NGLineInfo*);
  void HandleAtomicInline(const NGInlineItem&, NGLineInfo*);
  void HandleBlockInInline(const NGInlineItem&, NGLineInfo*);
  void ComputeMinMaxContentSizeForBlockChild(const NGInlineItem&,
                                             NGInlineItemResult*);

  bool CanBreakAfterAtomicInline(const NGInlineItem& item) const;
  bool CanBreakAfter(const NGInlineItem& item) const;
  // Returns true when text content at |offset| is
  //    kObjectReplacementCharacter (U+FFFC), or
  //    kNoBreakSpaceCharacter (U+00A0) if |sticky_images_quirk_|.
  bool MayBeAtomicInline(wtf_size_t offset) const;
  const NGInlineItem* TryGetAtomicInlineItemAfter(
      const NGInlineItem& item) const;

  void HandleFloat(const NGInlineItem&,
                   NGLineInfo*);
  void HandleInitialLetter(const NGInlineItem&, NGLineInfo*);
  void HandleOutOfFlowPositioned(const NGInlineItem&, NGLineInfo*);

  void HandleOpenTag(const NGInlineItem&, NGLineInfo*);
  void HandleCloseTag(const NGInlineItem&, NGLineInfo*);

  bool HandleOverflowIfNeeded(NGLineInfo*);
  void HandleOverflow(NGLineInfo*);
  void RewindOverflow(unsigned new_end, NGLineInfo*);
  void Rewind(unsigned new_end, NGLineInfo*);
  void ResetRewindLoopDetector() { last_rewind_.reset(); }

  const ComputedStyle& ComputeCurrentStyle(unsigned item_result_index,
                                           NGLineInfo*) const;
  void SetCurrentStyle(const ComputedStyle&);

  bool IsPreviousItemOfType(NGInlineItem::NGInlineItemType);
  void MoveToNextOf(const NGInlineItem&);
  void MoveToNextOf(const NGInlineItemResult&);

  void ComputeBaseDirection();
  void RecalcClonedBoxDecorations();

  LayoutUnit AvailableWidth() const {
    DCHECK_EQ(available_width_, ComputeAvailableWidth());
    return available_width_;
  }
  LayoutUnit AvailableWidthToFit() const {
    return AvailableWidth().AddEpsilon();
  }
  LayoutUnit RemainingAvailableWidth() const {
    return AvailableWidthToFit() - position_;
  }
  bool CanFitOnLine() const { return position_ <= AvailableWidthToFit(); }
  LayoutUnit ComputeAvailableWidth() const;

  void ClearNeedsLayout(const NGInlineItem& item);

  // True if the current line is hyphenated.
  bool HasHyphen() const { return hyphen_index_.has_value(); }
  LayoutUnit AddHyphen(NGInlineItemResults* item_results,
                       wtf_size_t index,
                       NGInlineItemResult* item_result);
  LayoutUnit AddHyphen(NGInlineItemResults* item_results, wtf_size_t index);
  LayoutUnit AddHyphen(NGInlineItemResults* item_results,
                       NGInlineItemResult* item_result);
  LayoutUnit RemoveHyphen(NGInlineItemResults* item_results);
  void RestoreLastHyphen(NGInlineItemResults* item_results);
  void FinalizeHyphen(NGInlineItemResults* item_results);

  // Create an NGInlineBreakToken for the last line returned by NextLine().
  // Only call once per instance.
  const NGInlineBreakToken* CreateBreakToken(const NGLineInfo&);

  // Represents the current offset of the input.
  LineBreakState state_;
  unsigned item_index_ = 0;
  unsigned offset_ = 0;
  unsigned svg_addressable_offset_ = 0;

  // |WhitespaceState| of the current end. When a line is broken, this indicates
  // the state of trailing whitespaces.
  WhitespaceState trailing_whitespace_;

  // The current position from inline_start. Unlike NGInlineLayoutAlgorithm
  // that computes position in visual order, this position in logical order.
  LayoutUnit position_;
  LayoutUnit available_width_;
  NGLineLayoutOpportunity line_opportunity_;

  NGInlineNode node_;

  NGLineBreakerMode mode_;

  // True if node_ is an initial letter box.
  const bool is_initial_letter_box_;

  // True if node_ is an SVG <text>.
  const bool is_svg_text_;

  // True if node_ is LayoutNGTextCombine.
  const bool is_text_combine_;

  // True if this line is the "first formatted line".
  // https://www.w3.org/TR/CSS22/selector.html#first-formatted-line
  bool is_first_formatted_line_ = false;

  bool use_first_line_style_ = false;

  // True when current box allows line wrapping.
  bool auto_wrap_ = false;

  // True when current box has 'word-break/word-wrap: break-word'.
  bool break_anywhere_if_overflow_ = false;

  // Force LineBreakType::kBreakCharacter by ignoring the current style if
  // |break_anywhere_if_overflow_| is set. Set to find grapheme cluster
  // boundaries for 'break-word' after overflow.
  bool override_break_anywhere_ = false;

  // True when breaking at soft hyphens (U+00AD) is allowed.
  bool enable_soft_hyphen_ = true;

  // True when the line should be non-empty if |IsLastLine|..
  bool force_non_empty_if_last_line_ = false;

  // Set when the line ended with a forced break. Used to setup the states for
  // the next line.
  bool is_after_forced_break_ = false;

  // Set in quirks mode when we're not supposed to break inside table cells
  // between images, and between text and images.
  bool sticky_images_quirk_ = false;

  // True if the resultant line contains a RubyRun with inline-end overhang.
  bool maybe_have_end_overhang_ = false;

  // True if ShouldCreateNewSvgSegment() should be called.
  bool needs_svg_segmentation_ = false;

  // True if we need to establish a new parallel flow for contents inside a
  // block-in-inline that overflowed the fragmentainer (although the
  // block-in-inline itself didn't overflow).
  bool needs_new_parallel_flow_ = false;

#if DCHECK_IS_ON()
  bool has_considered_creating_break_token_ = false;
#endif

  const NGInlineItemsData& items_data_;

  // The text content of this node. This is same as |items_data_.text_content|
  // except when sticky images quirk is needed. See
  // |NGInlineNode::TextContentForContentSize|.
  String text_content_;

  const NGConstraintSpace& constraint_space_;
  NGExclusionSpace* exclusion_space_;
  const NGInlineBreakToken* break_token_;
  const NGColumnSpannerPath* column_spanner_path_;
  scoped_refptr<const ComputedStyle> current_style_;

  LazyLineBreakIterator break_iterator_;
  HarfBuzzShaper shaper_;
  ShapeResultSpacing<String> spacing_;
  bool previous_line_had_forced_break_ = false;
  const Hyphenation* hyphenation_ = nullptr;

  absl::optional<wtf_size_t> hyphen_index_;
  bool has_any_hyphens_ = false;

  // Cache the result of |ComputeTrailingCollapsibleSpace| to avoid shaping
  // multiple times.
  struct TrailingCollapsibleSpace {
    NGInlineItemResult* item_result;
    scoped_refptr<const ShapeResultView> collapsed_shape_result;
  };
  absl::optional<TrailingCollapsibleSpace> trailing_collapsible_space_;

  // Keep track of handled float items. See HandleFloat().
  const NGPositionedFloatVector& leading_floats_;
  unsigned leading_floats_index_ = 0u;
  unsigned handled_leading_floats_index_;

  // Cache for computing |MinMaxSize|. See |MaxSizeCache|.
  MaxSizeCache* max_size_cache_ = nullptr;

  bool* depends_on_block_constraints_out_ = nullptr;

  // Keep the last item |HandleTextForFastMinContent()| has handled. This is
  // used to fallback the last word to |HandleText()|.
  const NGInlineItem* fast_min_content_item_ = nullptr;

  // The current base direction for the bidi algorithm.
  // This is copied from NGInlineNode, then updated after each forced line break
  // if 'unicode-bidi: plaintext'.
  TextDirection base_direction_;

  HeapVector<Member<const NGBlockBreakToken>> propagated_break_tokens_;

  // Fields for `box-decoration-break: clone`.
  unsigned cloned_box_decorations_count_ = 0;
  LayoutUnit cloned_box_decorations_initial_size_;
  LayoutUnit cloned_box_decorations_end_size_;
  bool has_cloned_box_decorations_ = false;

  // These fields are to detect rewind-loop.
  struct RewindIndex {
    wtf_size_t from_item_index;
    wtf_size_t to_index;
  };
  absl::optional<RewindIndex> last_rewind_;

  // This has a valid object if is_svg_text_.
  std::unique_ptr<ResolvedTextLayoutAttributesIterator> svg_resolved_iterator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BREAKER_H_
