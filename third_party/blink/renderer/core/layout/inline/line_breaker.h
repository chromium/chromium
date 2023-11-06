// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_LINE_BREAKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_LINE_BREAKER_H_

#include "base/check_op.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/exclusions/line_layout_opportunity.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_text_index.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/leading_floats.h"
#include "third_party/blink/renderer/core/layout/inline/line_break_point.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Hyphenation;
class InlineBreakToken;
class InlineItem;
class LineBreakCandidateContext;
class LineInfo;
class NGColumnSpannerPath;
class ResolvedTextLayoutAttributesIterator;
class ShapingLineBreaker;

// The line breaker needs to know which mode its in to properly handle floats.
enum class LineBreakerMode { kContent, kMinContent, kMaxContent };

// Represents a line breaker.
//
// This class measures each InlineItem and determines items to form a line,
// so that InlineLayoutAlgorithm can build a line box from the output.
class CORE_EXPORT LineBreaker {
  STACK_ALLOCATED();

 public:
  LineBreaker(InlineNode,
              LineBreakerMode,
              const NGConstraintSpace&,
              const LineLayoutOpportunity&,
              const LeadingFloats& leading_floats,
              const InlineBreakToken*,
              const NGColumnSpannerPath*,
              ExclusionSpace*);
  ~LineBreaker();

  const InlineItemsData& ItemsData() const { return items_data_; }

  // True if the last line has `box-decoration-break: clone`, which affected the
  // size.
  bool HasClonedBoxDecorations() const { return has_cloned_box_decorations_; }

  // Compute the next line break point and produces InlineItemResults for
  // the line.
  void NextLine(LineInfo*);

  bool IsFinished() const { return current_.item_index >= Items().size(); }

  // True if there are items that `ScoreLineBreaker` doesn't support.
  // Conditions that can be determined by `CollectInlines` are done by
  // `InlineNode::IsScoreLineBreakDisabled()`, but some conditions can change
  // withoiut `CollectInlines`. They are determined by this.
  bool ShouldDisableScoreLineBreak() const { return disable_score_line_break_; }

  void SetLineOpportunity(const LineLayoutOpportunity& line_opportunity);
  // Override the available width to compute line breaks. This is reset after
  // each `NextLine`.
  void OverrideAvailableWidth(LayoutUnit available_width);
  // Specify to break at the `offset` rather than the available width.
  void SetBreakAt(const LineBreakPoint& offset);

  // Computing |LineBreakerMode::kMinContent| with |MaxSizeCache| caches
  // information that can help computing |kMaxContent|. It is recommended to set
  // this when computing both |kMinContent| and |kMaxContent|.
  using MaxSizeCache = Vector<LayoutUnit, 64>;
  void SetIntrinsicSizeOutputs(MaxSizeCache* max_size_cache,
                               bool* depends_on_block_constraints_out);

  // Compute InlineItemResult for an open tag item.
  // Returns true if this item has edge and may have non-zero inline size.
  static bool ComputeOpenTagResult(const InlineItem&,
                                   const NGConstraintSpace&,
                                   bool is_in_svg_text,
                                   InlineItemResult*);

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

  // Find break candidates in the `item_result` and append to `context`. See
  // `NGLineBreakCandidate` and `NGLineBreakCandidateContext` for more details.
  void AppendCandidates(const InlineItemResult& item_result,
                        const LineInfo& line_info,
                        LineBreakCandidateContext& context);

  // True if the argument can break; i.e. has at least one break opportunity.
  bool CanBreakInside(const LineInfo& line_info);
  bool CanBreakInside(const InlineItemResult& item_result);

 private:
  Document& GetDocument() const { return node_.GetDocument(); }

  const String& Text() const { return text_content_; }
  const HeapVector<InlineItem>& Items() const { return items_data_.items; }

  String TextContentForLineBreak() const;

  InlineItemResult* AddItem(const InlineItem&, unsigned end_offset, LineInfo*);
  InlineItemResult* AddItem(const InlineItem&, LineInfo*);
  InlineItemResult* AddEmptyItem(const InlineItem&, LineInfo*);

  void BreakLine(LineInfo*);
  void PrepareNextLine(LineInfo*);

  void ComputeLineLocation(LineInfo*) const;

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

  void HandleText(const InlineItem& item, const ShapeResult&, LineInfo*);
  // Split |item| into segments, and add them to |line_info|.
  // This is for SVG <text>.
  void SplitTextIntoSegments(const InlineItem& item, LineInfo* line_info);
  // Returns true if we should split InlineItem before
  // svg_addressable_offset_.
  bool ShouldCreateNewSvgSegment() const;
  enum BreakResult { kSuccess, kOverflow, kBreakAt };
  BreakResult BreakText(InlineItemResult*,
                        const InlineItem&,
                        const ShapeResult&,
                        LayoutUnit available_width,
                        LayoutUnit available_width_with_hyphens,
                        LineInfo*);
  bool BreakTextAt(InlineItemResult*,
                   const InlineItem&,
                   ShapingLineBreaker& breaker,
                   LineInfo*);
  bool BreakTextAtPreviousBreakOpportunity(InlineItemResult* item_result);
  bool HandleTextForFastMinContent(InlineItemResult*,
                                   const InlineItem&,
                                   const ShapeResult&,
                                   LineInfo*);
  void HandleEmptyText(const InlineItem& item, LineInfo*);

  scoped_refptr<ShapeResultView> TruncateLineEndResult(const LineInfo&,
                                                       const InlineItemResult&,
                                                       unsigned end_offset);
  void UpdateShapeResult(const LineInfo&, InlineItemResult*);
  scoped_refptr<ShapeResult> ShapeText(const InlineItem&,
                                       unsigned start,
                                       unsigned end,
                                       ShapeOptions = ShapeOptions());

  void HandleTrailingSpaces(const InlineItem&, LineInfo*);
  void HandleTrailingSpaces(const InlineItem&, const ShapeResult*, LineInfo*);
  void RemoveTrailingCollapsibleSpace(LineInfo*);
  LayoutUnit TrailingCollapsibleSpaceWidth(LineInfo*);
  void ComputeTrailingCollapsibleSpace(LineInfo*);
  void RewindTrailingOpenTags(LineInfo*);

  void HandleControlItem(const InlineItem&, LineInfo*);
  void HandleForcedLineBreak(const InlineItem*, LineInfo*);
  void HandleBidiControlItem(const InlineItem&, LineInfo*);
  void HandleAtomicInline(const InlineItem&, LineInfo*);
  void HandleBlockInInline(const InlineItem&,
                           const NGBlockBreakToken*,
                           LineInfo*);
  void ComputeMinMaxContentSizeForBlockChild(const InlineItem&,
                                             InlineItemResult*);

  bool CanBreakAfterAtomicInline(const InlineItem& item) const;
  bool CanBreakAfter(const InlineItem& item) const;
  // Returns true when text content at |offset| is
  //    kObjectReplacementCharacter (U+FFFC), or
  //    kNoBreakSpaceCharacter (U+00A0) if |sticky_images_quirk_|.
  bool MayBeAtomicInline(wtf_size_t offset) const;
  const InlineItem* TryGetAtomicInlineItemAfter(const InlineItem& item) const;

  bool ShouldPushFloatAfterLine(NGUnpositionedFloat*, LineInfo*);
  void HandleFloat(const InlineItem&,
                   const NGBlockBreakToken* float_break_token,
                   LineInfo*);

  void HandleInitialLetter(const InlineItem&, LineInfo*);
  void HandleOutOfFlowPositioned(const InlineItem&, LineInfo*);

  void HandleOpenTag(const InlineItem&, LineInfo*);
  void HandleCloseTag(const InlineItem&, LineInfo*);

  bool HandleOverflowIfNeeded(LineInfo*);
  void HandleOverflow(LineInfo*);
  void RetryAfterOverflow(LineInfo*, InlineItemResults*);
  void RewindOverflow(unsigned new_end, LineInfo*);
  void Rewind(unsigned new_end, LineInfo*);
  void ResetRewindLoopDetector() { last_rewind_.reset(); }

  const ComputedStyle& ComputeCurrentStyle(unsigned item_result_index,
                                           LineInfo*) const;
  void SetCurrentStyle(const ComputedStyle&);
  void SetCurrentStyleForce(const ComputedStyle&);

  bool IsPreviousItemOfType(InlineItem::InlineItemType);
  void MoveToNextOf(const InlineItem&);
  void MoveToNextOf(const InlineItemResult&);

  void ComputeBaseDirection();
  void RecalcClonedBoxDecorations();

  LayoutUnit AvailableWidth() const { return available_width_; }
  LayoutUnit AvailableWidthToFit() const {
    return AvailableWidth().AddEpsilon();
  }
  LayoutUnit RemainingAvailableWidth() const {
    return AvailableWidthToFit() - position_;
  }
  bool CanFitOnLine() const { return position_ <= AvailableWidthToFit(); }
  void UpdateAvailableWidth();

  // True if the current line is hyphenated.
  bool HasHyphen() const { return hyphen_index_.has_value(); }
  LayoutUnit AddHyphen(InlineItemResults* item_results,
                       wtf_size_t index,
                       InlineItemResult* item_result);
  LayoutUnit AddHyphen(InlineItemResults* item_results, wtf_size_t index);
  LayoutUnit AddHyphen(InlineItemResults* item_results,
                       InlineItemResult* item_result);
  LayoutUnit RemoveHyphen(InlineItemResults* item_results);
  void RestoreLastHyphen(InlineItemResults* item_results);
  void FinalizeHyphen(InlineItemResults* item_results);

  // Create an InlineBreakToken for the last line returned by NextLine().
  // Only call once per instance.
  const InlineBreakToken* CreateBreakToken(const LineInfo&);

  // Represents the current offset of the input.
  LineBreakState state_;
  InlineItemTextIndex current_;
  unsigned svg_addressable_offset_ = 0;
  LineBreakPoint break_at_;

  // |WhitespaceState| of the current end. When a line is broken, this indicates
  // the state of trailing whitespaces.
  WhitespaceState trailing_whitespace_ = WhitespaceState::kUnknown;

  // The current position from inline_start. Unlike InlineLayoutAlgorithm
  // that computes position in visual order, this position in logical order.
  LayoutUnit position_;
  LayoutUnit available_width_;
  LineLayoutOpportunity line_opportunity_;

  InlineNode node_;

  LineBreakerMode mode_;

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

  // True when current box should fallback to break anywhere if it overflows.
  bool break_anywhere_if_overflow_ = false;

  // Force LineBreakType::kBreakCharacter by ignoring the current style if
  // |break_anywhere_if_overflow_| is set. Set to find grapheme cluster
  // boundaries for 'break-word' after overflow.
  bool override_break_anywhere_ = false;

  // Disable `LineBreakType::kPhrase` even if specified by the CSS.
  bool disable_phrase_ = false;

  bool disable_score_line_break_ = false;

  // True when the line should be non-empty if |IsLastLine|..
  bool force_non_empty_if_last_line_ = false;

  // Set when the line ended with a forced break. Used to setup the states for
  // the next line.
  bool is_after_forced_break_ = false;

  // Set in quirks mode when we're not supposed to break inside table cells
  // between images, and between text and images.
  bool sticky_images_quirk_ = false;

  // True if the resultant line contains a RubyColumn with inline-end overhang.
  bool maybe_have_end_overhang_ = false;

  // True if ShouldCreateNewSvgSegment() should be called.
  bool needs_svg_segmentation_ = false;

  // True if the block-in-inline broke inside, and it is to be resumed in the
  // same flow.
  bool resume_block_in_inline_in_same_flow_ = false;

#if DCHECK_IS_ON()
  bool has_considered_creating_break_token_ = false;
#endif

  const InlineItemsData& items_data_;

  // The text content of this node. This is same as |items_data_.text_content|
  // except when sticky images quirk is needed. See
  // |InlineNode::TextContentForContentSize|.
  String text_content_;

  const NGConstraintSpace& constraint_space_;
  ExclusionSpace* exclusion_space_;
  const InlineBreakToken* break_token_;
  const NGColumnSpannerPath* column_spanner_path_;
  const ComputedStyle* current_style_ = nullptr;

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
    InlineItemResult* item_result;
    scoped_refptr<const ShapeResultView> collapsed_shape_result;
  };
  absl::optional<TrailingCollapsibleSpace> trailing_collapsible_space_;

  LayoutUnit override_available_width_;

  // Keep track of handled float items. See HandleFloat().
  const LeadingFloats& leading_floats_;
  unsigned leading_floats_index_ = 0u;

  // Cache for computing |MinMaxSize|. See |MaxSizeCache|.
  MaxSizeCache* max_size_cache_ = nullptr;

  bool* depends_on_block_constraints_out_ = nullptr;

  // Keep the last item |HandleTextForFastMinContent()| has handled. This is
  // used to fallback the last word to |HandleText()|.
  const InlineItem* fast_min_content_item_ = nullptr;

  // The current base direction for the bidi algorithm.
  // This is copied from InlineNode, then updated after each forced line break
  // if 'unicode-bidi: plaintext'.
  TextDirection base_direction_;

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

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_LINE_BREAKER_H_
