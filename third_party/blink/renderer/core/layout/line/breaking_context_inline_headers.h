/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_BREAKING_CONTEXT_INLINE_HEADERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_BREAKING_CONTEXT_INLINE_HEADERS_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_box.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_ruby_run.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_text.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/line/inline_iterator.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/layout_text_info.h"
#include "third_party/blink/renderer/core/layout/line/line_breaker.h"
#include "third_party/blink/renderer/core/layout/line/line_info.h"
#include "third_party/blink/renderer/core/layout/line/line_width.h"
#include "third_party/blink/renderer/core/layout/line/trailing_objects.h"
#include "third_party/blink/renderer/core/layout/line/word_measurement.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/text/hyphenation.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// We don't let our line box tree for a single line get any deeper than this.
const unsigned kCMaxLineDepth = 200;

class BreakingContext {
  STACK_ALLOCATED();

 public:
  BreakingContext(InlineBidiResolver& resolver,
                  LineInfo& in_line_info,
                  LineWidth& line_width,
                  LayoutTextInfo& in_layout_text_info,
                  bool applied_start_width,
                  LineLayoutBlockFlow block)
      : resolver_(resolver),
        current_(resolver.GetPosition()),
        line_break_(resolver.GetPosition()),
        block_(block),
        last_object_(current_.GetLineLayoutItem()),
        next_object_(nullptr),
        current_style_(nullptr),
        block_style_(block.Style()),
        line_info_(in_line_info),
        layout_text_info_(in_layout_text_info),
        width_(line_width),
        curr_ws_(EWhiteSpace::kNormal),
        last_ws_(EWhiteSpace::kNormal),
        preserves_newline_(false),
        at_start_(true),
        ignoring_spaces_(false),
        current_character_is_space_(false),
        applied_start_width_(applied_start_width),
        include_end_width_(true),
        auto_wrap_(false),
        auto_wrap_was_ever_true_on_line_(false),
        floats_fit_on_line_(true),
        collapse_white_space_(false),
        starting_new_paragraph_(line_info_.PreviousLineBrokeCleanly()),
        at_end_(false),
        line_midpoint_state_(resolver.GetMidpointState()) {
    line_info_.SetPreviousLineBrokeCleanly(false);
  }

  LineLayoutItem CurrentItem() { return current_.GetLineLayoutItem(); }
  InlineIterator LineBreak() { return line_break_; }
  bool AtEnd() { return at_end_; }

  void InitializeForCurrentObject();

  void Increment();

  void HandleBR(EClear&);
  void HandleOutOfFlowPositioned(Vector<LineLayoutBox>& positioned_objects);
  void HandleFloat();
  void HandleEmptyInline();
  void HandleReplaced();
  bool HandleText(WordMeasurements&, bool& hyphenated);
  void PrepareForNextCharacter(const LineLayoutText&,
                               bool& prohibit_break_inside,
                               bool previous_character_is_space);
  bool CanBreakAtWhitespace(bool break_words,
                            WordMeasurement&,
                            bool stopped_ignoring_spaces,
                            float char_width,
                            bool& hyphenated,
                            bool disable_soft_hyphen,
                            float& hyphen_width,
                            bool between_words,
                            bool mid_word_break,
                            bool can_break_mid_word,
                            bool previous_character_is_space,
                            float last_width_measurement,
                            const LineLayoutText&,
                            const Font&,
                            bool apply_word_spacing,
                            float word_spacing);
  void TrailingSpacesHang(bool can_break_mid_word);
  bool TrailingSpaceExceedsAvailableWidth(bool can_break_mid_word,
                                          const LineLayoutText&,
                                          WordMeasurement&,
                                          bool apply_word_spacing,
                                          bool word_spacing,
                                          const Font&);
  WordMeasurement& CalculateWordWidth(WordMeasurements&,
                                      LineLayoutText&,
                                      unsigned last_space,
                                      float& last_width_measurement,
                                      float word_spacing_for_word_measurement,
                                      const Font&,
                                      float word_trailing_space_width,
                                      UChar);
  void StopIgnoringSpaces(unsigned& last_space);
  void CommitAndUpdateLineBreakIfNeeded();
  InlineIterator HandleEndOfLine();

  void ClearLineBreakIfFitsOnLine() {
    if (width_.FitsOnLine() || last_ws_ == EWhiteSpace::kNowrap)
      line_break_.Clear();
  }

 private:
  void SetCurrentCharacterIsSpace(UChar);
  void SkipTrailingWhitespace(InlineIterator&, const LineInfo&);
  bool ShouldMidWordBreak(UChar,
                          LineLayoutText,
                          const Font&,
                          unsigned& next_grapheme_cluster,
                          float& char_width,
                          float& width_from_last_breaking_opportunity,
                          bool break_all,
                          int& next_breakable_position_for_break_all);
  bool RewindToMidWordBreak(WordMeasurement&, int end, float width);
  bool CanMidWordBreakBefore(LineLayoutText);
  bool RewindToFirstMidWordBreak(LineLayoutText,
                                 const ComputedStyle&,
                                 const Font&,
                                 const LazyLineBreakIterator&,
                                 WordMeasurement&);
  bool RewindToMidWordBreak(LineLayoutText,
                            const ComputedStyle&,
                            const Font&,
                            LineBreakType line_break_type,
                            WordMeasurement&);
  bool Hyphenate(LineLayoutText,
                 const ComputedStyle&,
                 const Font&,
                 const Hyphenation&,
                 float last_space_word_spacing,
                 WordMeasurement&);
  bool IsBreakAtSoftHyphen() const;

  InlineBidiResolver& resolver_;

  InlineIterator current_;
  InlineIterator line_break_;
  InlineIterator start_of_ignored_spaces_;

  LineLayoutBlockFlow block_;
  LineLayoutItem last_object_;
  LineLayoutItem next_object_;

  const ComputedStyle* current_style_;
  const ComputedStyle* block_style_;

  LineInfo& line_info_;

  LayoutTextInfo& layout_text_info_;

  LineWidth width_;

  EWhiteSpace curr_ws_;
  EWhiteSpace last_ws_;

  bool preserves_newline_;
  bool at_start_;
  bool ignoring_spaces_;
  bool current_character_is_space_;
  bool previous_character_is_space_;
  bool has_former_opportunity_;
  unsigned current_start_offset_;  // initial offset for the current text
  bool applied_start_width_;
  bool include_end_width_;
  bool auto_wrap_;
  bool auto_wrap_was_ever_true_on_line_;
  bool floats_fit_on_line_;
  bool collapse_white_space_;
  bool starting_new_paragraph_;
  bool at_end_;

  LineMidpointState& line_midpoint_state_;

  TrailingObjects trailing_objects_;
};

// When ignoring spaces, this needs to be called for objects that need line
// boxes such as LayoutInlines or hard line breaks to ensure that they're not
// ignored.
inline void EnsureLineBoxInsideIgnoredSpaces(LineMidpointState* midpoint_state,
                                             LineLayoutItem item) {
  InlineIterator midpoint(nullptr, item, 0);
  midpoint_state->StopIgnoringSpaces(midpoint);
  midpoint_state->StartIgnoringSpaces(midpoint);
}

inline bool ShouldCollapseWhiteSpace(const ComputedStyle& style,
                                     const LineInfo& line_info,
                                     WhitespacePosition whitespace_position) {
  // CSS2 16.6.1
  // If a space (U+0020) at the beginning of a line has 'white-space' set to
  // 'normal', 'nowrap', or 'pre-line', it is removed.
  // If a space (U+0020) at the end of a line has 'white-space' set to 'normal',
  // 'nowrap', or 'pre-line', it is also removed.
  // If spaces (U+0020) or tabs (U+0009) at the end of a line have 'white-space'
  // set to 'pre-wrap', UAs may visually collapse them.
  return style.CollapseWhiteSpace() ||
         (whitespace_position == kTrailingWhitespace &&
          style.WhiteSpace() == EWhiteSpace::kPreWrap &&
          (!line_info.IsEmpty() || !line_info.PreviousLineBrokeCleanly()));
}

inline bool RequiresLineBoxForContent(LineLayoutInline flow,
                                      const LineInfo& line_info) {
  LineLayoutItem parent = flow.Parent();
  if (flow.GetDocument().InNoQuirksMode() &&
      (flow.Style(line_info.IsFirstLine())->LineHeight() !=
           parent.Style(line_info.IsFirstLine())->LineHeight() ||
       flow.StyleRef().VerticalAlign() != parent.StyleRef().VerticalAlign() ||
       !parent.StyleRef().HasIdenticalAscentDescentAndLineGap(flow.StyleRef())))
    return true;
  return false;
}

inline bool AlwaysRequiresLineBox(LineLayoutItem flow) {
  // FIXME: Right now, we only allow line boxes for inlines that are truly
  // empty. We need to fix this, though, because at the very least, inlines
  // containing only ignorable whitespace should should also have line boxes.
  return IsEmptyInline(flow) &&
         LineLayoutInline(flow).HasInlineDirectionBordersPaddingOrMargin();
}

inline bool RequiresLineBox(
    const InlineIterator& it,
    const LineInfo& line_info = LineInfo(),
    WhitespacePosition whitespace_position = kLeadingWhitespace) {
  if (it.GetLineLayoutItem().IsEmptyText())
    return false;

  if (it.GetLineLayoutItem().IsFloatingOrOutOfFlowPositioned())
    return false;

  if (it.GetLineLayoutItem().IsLayoutInline() &&
      !AlwaysRequiresLineBox(it.GetLineLayoutItem()) &&
      !RequiresLineBoxForContent(LineLayoutInline(it.GetLineLayoutItem()),
                                 line_info))
    return false;

  if (!ShouldCollapseWhiteSpace(it.GetLineLayoutItem().StyleRef(), line_info,
                                whitespace_position) ||
      it.GetLineLayoutItem().IsBR())
    return true;

  UChar current = it.Current();
  bool not_just_whitespace = current != kSpaceCharacter &&
                             current != kTabulationCharacter &&
                             current != kSoftHyphenCharacter &&
                             (current != kNewlineCharacter ||
                              it.GetLineLayoutItem().PreservesNewline());
  return not_just_whitespace || IsEmptyInline(it.GetLineLayoutItem());
}

inline void SetStaticPositions(LineLayoutBlockFlow block,
                               LineLayoutBox child,
                               IndentTextOrNot indent_text) {
  DCHECK(child.IsOutOfFlowPositioned());
  // FIXME: The math here is actually not really right. It's a best-guess
  // approximation that will work for the common cases
  LineLayoutItem container_block = child.Container();
  LayoutUnit block_height = block.LogicalHeight();
  if (container_block.IsLayoutInline()) {
    // A relative positioned inline encloses us. In this case, we also have to
    // determine our position as though we were an inline.
    // Set |staticInlinePosition| and |staticBlockPosition| on the relative
    // positioned inline so that we can obtain the value later.
    DCHECK(LineLayoutInline(container_block).Layer());
    LineLayoutInline(container_block)
        .Layer()
        ->SetStaticInlinePosition(
            block.StartAlignedOffsetForLine(block_height, indent_text));
    LineLayoutInline(container_block)
        .Layer()
        ->SetStaticBlockPosition(block_height);

    // If |child| is a leading or trailing positioned object this is its only
    // opportunity to ensure it moves with an inline container changing width.
    child.MoveWithEdgeOfInlineContainerIfNecessary(
        child.IsHorizontalWritingMode());
  }
  block.UpdateStaticInlinePositionForChild(child, block_height, indent_text);
  child.Layer()->SetStaticBlockPosition(block_height);
}

// FIXME: The entire concept of the skipTrailingWhitespace function is flawed,
// since we really need to be building line boxes even for containers that may
// ultimately collapse away. Otherwise we'll never get positioned elements quite
// right. In other words, we need to build this function's work into the normal
// line object iteration process. NB. this function will insert any floating
// elements that would otherwise be skipped but it will not position them.
inline void BreakingContext::SkipTrailingWhitespace(InlineIterator& iterator,
                                                    const LineInfo& line_info) {
  while (!iterator.AtEnd() &&
         !RequiresLineBox(iterator, line_info, kTrailingWhitespace)) {
    LineLayoutItem item = iterator.GetLineLayoutItem();
    if (item.IsOutOfFlowPositioned())
      SetStaticPositions(block_, LineLayoutBox(item), kDoNotIndentText);
    else if (item.IsFloating())
      block_.InsertFloatingObject(LineLayoutBox(item));
    iterator.Increment();
  }
}

inline void BreakingContext::InitializeForCurrentObject() {
  current_style_ = current_.GetLineLayoutItem().Style();
  next_object_ =
      BidiNextSkippingEmptyInlines(block_, current_.GetLineLayoutItem());
  if (next_object_ && next_object_.Parent() &&
      !next_object_.Parent().IsDescendantOf(
          current_.GetLineLayoutItem().Parent()))
    include_end_width_ = true;

  curr_ws_ =
      current_.GetLineLayoutItem().IsLayoutInline()
          ? current_style_->WhiteSpace()
          : current_.GetLineLayoutItem().Parent().StyleRef().WhiteSpace();
  last_ws_ = last_object_.IsLayoutInline()
                 ? last_object_.StyleRef().WhiteSpace()
                 : last_object_.Parent().StyleRef().WhiteSpace();

  bool is_svg_text = current_.GetLineLayoutItem().IsSVGInlineText();
  auto_wrap_ = !is_svg_text && ComputedStyle::AutoWrap(curr_ws_);
  auto_wrap_was_ever_true_on_line_ =
      auto_wrap_was_ever_true_on_line_ || auto_wrap_;

  preserves_newline_ = !is_svg_text && ComputedStyle::PreserveNewline(curr_ws_);

  collapse_white_space_ = ComputedStyle::CollapseWhiteSpace(curr_ws_);

  // Ensure the whitespace in constructions like '<span style="white-space:
  // pre-wrap">text <span><span> text</span>' does not collapse.
  if (collapse_white_space_ && !ComputedStyle::CollapseWhiteSpace(last_ws_))
    current_character_is_space_ = false;

  // Since current_ iterates all along the text's length, we need to store the
  // initial offset of the current handle text so that we can then identify
  // a single leading white-space as potential breaking opportunities.
  current_start_offset_ = current_.Offset();
  has_former_opportunity_ = false;

  if (curr_ws_ == EWhiteSpace::kBreakSpaces) {
    layout_text_info_.line_break_iterator_.SetBreakSpace(
        BreakSpaceType::kAfterEverySpace);
  }
}

inline void BreakingContext::Increment() {
  current_.MoveToStartOf(next_object_);

  // When the line box tree is created, this position in the line will be
  // snapped to LayoutUnit's, and those measurements will be used by the paint
  // code. Do the equivalent snapping here, to get consistent line measurements.
  width_.SnapAtNodeBoundary();

  at_start_ = false;
}

inline void BreakingContext::HandleBR(EClear& clear) {
  if (width_.FitsOnLine()) {
    LineLayoutItem br = current_.GetLineLayoutItem();
    line_break_.MoveToStartOf(br);
    line_break_.Increment();

    // A <br> always breaks a line, so don't let the line be collapsed
    // away. Also, the space at the end of a line with a <br> does not
    // get collapsed away. It only does this if the previous line broke
    // cleanly. Otherwise the <br> has no effect on whether the line is
    // empty or not.
    if (starting_new_paragraph_)
      line_info_.SetEmpty(false);
    trailing_objects_.Clear();
    line_info_.SetPreviousLineBrokeCleanly(true);

    // A <br> with clearance always needs a linebox in case the lines below it
    // get dirtied later and need to check for floats to clear - so if we're
    // ignoring spaces, stop ignoring them and add a run for this object.
    if (ignoring_spaces_ && current_style_->HasClear())
      EnsureLineBoxInsideIgnoredSpaces(&line_midpoint_state_, br);

    if (!line_info_.IsEmpty())
      clear = current_style_->Clear(block_.StyleRef());
  }
  at_end_ = true;
}

inline LayoutUnit BorderPaddingMarginStart(LineLayoutInline child) {
  return child.MarginStart() + child.PaddingStart() + child.BorderStart();
}

inline LayoutUnit BorderPaddingMarginEnd(LineLayoutInline child) {
  return child.MarginEnd() + child.PaddingEnd() + child.BorderEnd();
}

enum CollapsibleWhiteSpace {
  IgnoreCollapsibleWhiteSpace,
  UseCollapsibleWhiteSpace
};

inline bool ShouldAddBorderPaddingMargin(LineLayoutItem child,
                                         bool& check_side,
                                         CollapsibleWhiteSpace white_space) {
  if (!child)
    return true;
  if (child.IsText()) {
    // A caller will only be interested in adding BPM from objects separated by
    // collapsible whitespace if they haven't already been added to the line's
    // width, such as when adding end-BPM when about to place a float after a
    // linebox.
    if (white_space == UseCollapsibleWhiteSpace &&
        LineLayoutText(child).IsAllCollapsibleWhitespace())
      return true;
    if (!LineLayoutText(child).TextLength())
      return true;
  }
  check_side = false;
  return check_side;
}

inline LayoutUnit InlineLogicalWidthFromAncestorsIfNeeded(
    LineLayoutItem child,
    bool start = true,
    bool end = true,
    CollapsibleWhiteSpace white_space = IgnoreCollapsibleWhiteSpace) {
  unsigned line_depth = 1;
  LayoutUnit extra_width;
  LineLayoutItem parent = child.Parent();
  while (parent.IsLayoutInline() && line_depth++ < kCMaxLineDepth) {
    LineLayoutInline parent_as_layout_inline(parent);
    if (!IsEmptyInline(parent_as_layout_inline)) {
      if (start && ShouldAddBorderPaddingMargin(child.PreviousSibling(), start,
                                                white_space))
        extra_width += BorderPaddingMarginStart(parent_as_layout_inline);
      if (end &&
          ShouldAddBorderPaddingMargin(child.NextSibling(), end, white_space))
        extra_width += BorderPaddingMarginEnd(parent_as_layout_inline);
      if (!start && !end)
        return extra_width;
    }
    child = parent;
    parent = child.Parent();
  }
  return extra_width;
}

inline void BreakingContext::HandleOutOfFlowPositioned(
    Vector<LineLayoutBox>& positioned_objects) {
  // If our original display wasn't an inline type, then we can
  // go ahead and determine our static inline position now.
  LineLayoutBox box(current_.GetLineLayoutItem());
  bool is_inline_type = box.StyleRef().IsOriginalDisplayInlineType();
  if (!is_inline_type) {
    block_.SetStaticInlinePositionForChild(box, block_.StartOffsetForContent());
  } else {
    // If our original display was an INLINE type, then we can determine our
    // static y position now. Note, however, that if we're paginated, we may
    // have to update this position after the line has been laid out, since the
    // line may be pushed by a pagination strut.
    box.Layer()->SetStaticBlockPosition(block_.LogicalHeight());
  }

  // If we're ignoring spaces, we have to stop and include this object and
  // then start ignoring spaces again.
  bool container_is_inline = box.Container().IsLayoutInline();
  if (is_inline_type || container_is_inline) {
    if (ignoring_spaces_)
      EnsureLineBoxInsideIgnoredSpaces(&line_midpoint_state_, box);
    trailing_objects_.AppendObjectIfNeeded(box);
  }
  if (!container_is_inline)
    positioned_objects.push_back(box);
  width_.AddUncommittedWidth(
      InlineLogicalWidthFromAncestorsIfNeeded(box).ToFloat());
  // Reset prior line break context characters.
  layout_text_info_.line_break_iterator_.ResetPriorContext();
}

inline void BreakingContext::HandleFloat() {
  LineLayoutBox float_box(current_.GetLineLayoutItem());
  FloatingObject* floating_object = block_.InsertFloatingObject(float_box);

  if (floats_fit_on_line_) {
    // We need to calculate the logical width of the float before we can tell
    // whether it's going to fit on the line. That means that we need to
    // position and lay it out. Note that we have to avoid positioning floats
    // that have been placed prematurely: Sometimes, floats are inserted too
    // early by skipTrailingWhitespace(), and later on they all get placed by
    // the first float here in handleFloat(). Their position may then be wrong,
    // but it's too late to do anything about that now. See crbug.com/671577
    if (!floating_object->IsPlaced()) {
      LayoutUnit logical_top = block_.LogicalHeight();
      if (const FloatingObject* last_placed_float = block_.LastPlacedFloat()) {
        logical_top = std::max(logical_top,
                               block_.LogicalTopForFloat(*last_placed_float));
      }
      block_.PositionAndLayoutFloat(*floating_object, logical_top);
    }

    // Check if it fits in the current line; if it does, place it now,
    // otherwise, place it after moving to next line (in newLine() func).
    // FIXME: Bug 110372: Properly position multiple stacked floats with
    // non-rectangular shape outside.
    // When fitting the float on the line we need to treat the width on the line
    // so far as though end-border, -padding and -margin from
    // inline ancestors has been applied to the end of the previous inline box.
    float width_from_ancestors =
        InlineLogicalWidthFromAncestorsIfNeeded(float_box, false, true,
                                                UseCollapsibleWhiteSpace)
            .ToFloat();
    width_.AddUncommittedWidth(width_from_ancestors);
    if (width_.FitsOnLine(
            block_.LogicalWidthForFloat(*floating_object).ToFloat(),
            kExcludeWhitespace)) {
      block_.PlaceNewFloats(block_.LogicalHeight(), &width_);
      if (line_break_.GetLineLayoutItem() == current_.GetLineLayoutItem()) {
        DCHECK(!line_break_.Offset());
        line_break_.Increment();
      }
    } else {
      floats_fit_on_line_ = false;
    }
    width_.AddUncommittedWidth(-width_from_ancestors);
  }
  // Update prior line break context characters, using U+FFFD (OBJECT
  // REPLACEMENT CHARACTER) for floating element.
  layout_text_info_.line_break_iterator_.UpdatePriorContext(
      kReplacementCharacter);
}

// This is currently just used for list markers and inline flows that have line
// boxes. Neither should have an effect on whitespace at the start of the line.
inline bool ShouldSkipWhitespaceAfterStartObject(
    LineLayoutBlockFlow block,
    LineLayoutItem o,
    LineMidpointState& line_midpoint_state) {
  LineLayoutItem next = BidiNextSkippingEmptyInlines(block, o);
  while (next && next.IsFloatingOrOutOfFlowPositioned())
    next = BidiNextSkippingEmptyInlines(block, next);

  while (next && IsEmptyInline(next)) {
    LineLayoutItem child = LineLayoutInline(next).FirstChild();
    next = child ? child : BidiNextSkippingEmptyInlines(block, next);
  }

  if (next && !next.IsBR() && next.IsText() &&
      LineLayoutText(next).TextLength() > 0) {
    LineLayoutText next_text(next);
    UChar next_char = next_text.CharacterAt(0);
    if (next_text.StyleRef().IsCollapsibleWhiteSpace(next_char)) {
      line_midpoint_state.StartIgnoringSpaces(InlineIterator(nullptr, o, 0));
      return true;
    }
  }

  return false;
}

inline void BreakingContext::HandleEmptyInline() {
  // This should only end up being called on empty inlines
  DCHECK(current_.GetLineLayoutItem());

  LineLayoutInline flow_box(current_.GetLineLayoutItem());

  bool requires_line_box = AlwaysRequiresLineBox(current_.GetLineLayoutItem());
  if (requires_line_box || RequiresLineBoxForContent(flow_box, line_info_)) {
    // An empty inline that only has line-height, vertical-align or font-metrics
    // will not force linebox creation (and thus affect the height of the line)
    // if the rest of the line is empty.
    if (requires_line_box)
      line_info_.SetEmpty(false);
    if (ignoring_spaces_) {
      // If we are in a run of ignored spaces then ensure we get a linebox if
      // lineboxes are eventually created for the line...
      trailing_objects_.Clear();
      EnsureLineBoxInsideIgnoredSpaces(&line_midpoint_state_,
                                       current_.GetLineLayoutItem());
    } else if (block_style_->CollapseWhiteSpace() &&
               resolver_.GetPosition().GetLineLayoutItem() ==
                   current_.GetLineLayoutItem() &&
               ShouldSkipWhitespaceAfterStartObject(
                   block_, current_.GetLineLayoutItem(),
                   line_midpoint_state_)) {
      // If this object is at the start of the line, we need to behave like list
      // markers and start ignoring spaces.
      current_character_is_space_ = true;
      ignoring_spaces_ = true;
    } else {
      // If we are after a trailing space but aren't ignoring spaces yet then
      // ensure we get a linebox if we encounter collapsible whitepace.
      trailing_objects_.AppendObjectIfNeeded(current_.GetLineLayoutItem());
    }
  }

  width_.AddUncommittedWidth(
      (InlineLogicalWidthFromAncestorsIfNeeded(current_.GetLineLayoutItem()) +
       BorderPaddingMarginStart(flow_box) + BorderPaddingMarginEnd(flow_box))
          .ToFloat());
}

inline void BreakingContext::HandleReplaced() {
  LineLayoutBox replaced_box(current_.GetLineLayoutItem());

  if (at_start_)
    width_.UpdateAvailableWidth(replaced_box.LogicalHeight());

  // Break on replaced elements if either has normal white-space,
  // or if the replaced element is ruby that can break before.
  if ((auto_wrap_ || ComputedStyle::AutoWrap(last_ws_)) &&
      (!current_.GetLineLayoutItem().IsRubyRun() ||
       LineLayoutRubyRun(current_.GetLineLayoutItem())
           .CanBreakBefore(layout_text_info_.line_break_iterator_))) {
    width_.Commit();
    line_break_.MoveToStartOf(current_.GetLineLayoutItem());
  }

  if (ignoring_spaces_) {
    line_midpoint_state_.StopIgnoringSpaces(
        InlineIterator(nullptr, current_.GetLineLayoutItem(), 0));
  }
  line_info_.SetEmpty(false);
  ignoring_spaces_ = false;
  current_character_is_space_ = false;
  trailing_objects_.Clear();

  // Optimize for a common case. If we can't find whitespace after the list
  // item, then this is all moot.
  LayoutUnit replaced_logical_width =
      block_.LogicalWidthForChild(replaced_box) +
      block_.MarginStartForChild(replaced_box) +
      block_.MarginEndForChild(replaced_box) +
      InlineLogicalWidthFromAncestorsIfNeeded(current_.GetLineLayoutItem());
  if (current_.GetLineLayoutItem().IsListMarker()) {
    if (block_style_->CollapseWhiteSpace() &&
        ShouldSkipWhitespaceAfterStartObject(
            block_, current_.GetLineLayoutItem(), line_midpoint_state_)) {
      // Like with inline flows, we start ignoring spaces to make sure that any
      // additional spaces we see will be discarded.
      current_character_is_space_ = true;
      ignoring_spaces_ = true;
    }
    if (LineLayoutListMarker(current_.GetLineLayoutItem()).IsInside())
      width_.AddUncommittedWidth(replaced_logical_width.ToFloat());
  } else {
    width_.AddUncommittedWidth(replaced_logical_width.ToFloat());
  }
  if (current_.GetLineLayoutItem().IsRubyRun())
    width_.ApplyOverhang(LineLayoutRubyRun(current_.GetLineLayoutItem()),
                         last_object_, next_object_);
  // Update prior line break context characters, using U+FFFD (OBJECT
  // REPLACEMENT CHARACTER) for replaced element.
  layout_text_info_.line_break_iterator_.UpdatePriorContext(
      kReplacementCharacter);
}

inline void NextCharacter(UChar& current_character,
                          UChar& last_character,
                          UChar& second_to_last_character) {
  second_to_last_character = last_character;
  last_character = current_character;
}

ALWAYS_INLINE void BreakingContext::SetCurrentCharacterIsSpace(UChar c) {
  current_character_is_space_ =
      c == kSpaceCharacter || c == kTabulationCharacter ||
      (!preserves_newline_ && (c == kNewlineCharacter));
}

inline float FirstPositiveWidth(const WordMeasurements& word_measurements) {
  for (const WordMeasurement& word_measurement : word_measurements) {
    if (word_measurement.width > 0)
      return word_measurement.width;
  }
  return 0;
}

ALWAYS_INLINE TextDirection
TextDirectionFromUnicode(WTF::unicode::CharDirection direction) {
  return direction == WTF::unicode::kRightToLeft ||
                 direction == WTF::unicode::kRightToLeftArabic
             ? TextDirection::kRtl
             : TextDirection::kLtr;
}

ALWAYS_INLINE float TextWidth(
    LineLayoutText text,
    unsigned from,
    unsigned len,
    const Font& font,
    float x_pos,
    bool collapse_white_space,
    HashSet<const SimpleFontData*>* fallback_fonts = nullptr,
    FloatRect* glyph_bounds = nullptr) {
  if ((!from && len == text.TextLength()) || text.StyleRef().HasTextCombine()) {
    return text.Width(from, len, font, LayoutUnit(x_pos),
                      text.StyleRef().Direction(), fallback_fonts,
                      glyph_bounds);
  }

  TextRun run = ConstructTextRun(font, text, from, len, text.StyleRef());
  run.SetTabSize(!collapse_white_space, text.StyleRef().GetTabSize());
  run.SetXPos(x_pos);
  return font.Width(run, fallback_fonts, glyph_bounds);
}

ALWAYS_INLINE bool BreakingContext::ShouldMidWordBreak(
    UChar c,
    LineLayoutText layout_text,
    const Font& font,
    unsigned& next_grapheme_cluster,
    float& char_width,
    float& width_from_last_breaking_opportunity,
    bool break_all,
    int& next_breakable_position_for_break_all) {
  // For breakWords/breakAll, we need to measure up to normal break
  // opportunity and then rewindToMidWordBreak() because ligatures/kerning can
  // shorten the width as we add more characters.
  // However, doing so can hit the performance when a "word" is really long,
  // such as minimized JS, because the next line will re-shape the rest of the
  // word in the current architecture.
  // This function is a heuristic optimization to stop at 2em overflow.
  float overflow_allowance = 2 * font.GetFontDescription().ComputedSize();

  width_from_last_breaking_opportunity += char_width;
  unsigned char_len;
  if (layout_text.Is8Bit()) {
    // For 8-bit string, code unit == grapheme cluster.
    char_len = 1;
  } else {
    NonSharedCharacterBreakIterator iterator(layout_text.GetText());
    int next = iterator.Following(current_.Offset());
    next_grapheme_cluster =
        next != kTextBreakDone ? next : layout_text.TextLength();
    char_len = next_grapheme_cluster - current_.Offset();
  }
  char_width =
      TextWidth(layout_text, current_.Offset(), char_len, font,
                width_.CommittedWidth() + width_from_last_breaking_opportunity,
                collapse_white_space_);
  if (width_.CommittedWidth() + width_from_last_breaking_opportunity +
          char_width <=
      width_.AvailableWidth() + overflow_allowance)
    return false;

  // breakAll has different break opportunities. Ensure we break only at
  // breakAll allows to break.
  if (break_all && !layout_text_info_.line_break_iterator_.IsBreakable(
                       current_.Offset(), next_breakable_position_for_break_all,
                       LineBreakType::kBreakAll)) {
    return false;
  }

  return true;
}

ALWAYS_INLINE bool BreakingContext::RewindToMidWordBreak(
    WordMeasurement& word_measurement,
    int end,
    float width) {
  word_measurement.end_offset = end;
  word_measurement.width = width;

  current_.MoveTo(current_.GetLineLayoutItem(), end,
                  current_.NextBreakablePosition());
  SetCurrentCharacterIsSpace(current_.Current());
  line_break_.MoveTo(current_.GetLineLayoutItem(), end,
                     current_.NextBreakablePosition());
  return true;
}

ALWAYS_INLINE bool BreakingContext::CanMidWordBreakBefore(LineLayoutText text) {
  // No break opportunities at the beginning of a line.
  if (!width_.CurrentWidth())
    return false;
  // If this text node is not the first child of an inline, the parent and the
  // preivous have the same style.
  if (text.PreviousSibling())
    return true;
  // Element boundaries belong to the parent of the element.
  // TODO(kojii): Not all cases of element boundaries are supported yet.
  // crbug.com/282134
  if (LineLayoutItem element = text.Parent()) {
    if (LineLayoutItem parent = element.Parent()) {
      const ComputedStyle& parent_style = parent.StyleRef();
      return parent_style.AutoWrap() &&
             ((parent_style.BreakWords() && !width_.CommittedWidth()) ||
              parent_style.WordBreak() == EWordBreak::kBreakAll);
    }
  }
  return false;
}

ALWAYS_INLINE bool BreakingContext::RewindToFirstMidWordBreak(
    LineLayoutText text,
    const ComputedStyle& style,
    const Font& font,
    const LazyLineBreakIterator& break_iterator,
    WordMeasurement& word_measurement) {
  int start = word_measurement.start_offset;
  int end = CanMidWordBreakBefore(text)
                ? start
                : break_iterator.NextBreakOpportunity(start + 1);
  if (end >= word_measurement.end_offset)
    return false;

  float width = TextWidth(text, start, end - start, font, width_.CurrentWidth(),
                          collapse_white_space_);
  // If the first break opportunity doesn't fit, and if there's a break
  // opportunity in previous runs, break at the opportunity.
  if (!width_.FitsOnLine(width) &&
      (width_.CommittedWidth() || has_former_opportunity_))
    return false;
  return RewindToMidWordBreak(word_measurement, end, width);
}

ALWAYS_INLINE bool BreakingContext::RewindToMidWordBreak(
    LineLayoutText text,
    const ComputedStyle& style,
    const Font& font,
    LineBreakType line_break_type,
    WordMeasurement& word_measurement) {
  int start = word_measurement.start_offset;
  int len = word_measurement.end_offset - start;
  if (!len)
    return false;

  LazyLineBreakIterator break_iterator(
      text.GetText(), style.LocaleForLineBreakIterator(), line_break_type);
  if (curr_ws_ == EWhiteSpace::kBreakSpaces)
    break_iterator.SetBreakSpace(BreakSpaceType::kAfterEverySpace);
  float x_pos_to_break = width_.AvailableWidth() - width_.CurrentWidth();
  if (x_pos_to_break <= LayoutUnit::Epsilon()) {
    // There were no space left. Skip computing how many characters can fit.
    return RewindToFirstMidWordBreak(text, style, font, break_iterator,
                                     word_measurement);
  }

  TextRun run = ConstructTextRun(font, text, start, len, style);
  run.SetTabSize(!collapse_white_space_, style.GetTabSize());
  run.SetXPos(width_.CurrentWidth());

  // TODO(kojii): should be replaced with safe-to-break when hb is ready.
  x_pos_to_break += LayoutUnit::Epsilon();
  if (run.Rtl())
    x_pos_to_break = word_measurement.width - x_pos_to_break;
  len = font.OffsetForPosition(run, x_pos_to_break, OnlyFullGlyphs,
                               DontBreakGlyphs);
  int end = start + len;
  if (len) {
    end = break_iterator.PreviousBreakOpportunity(end, start);
    len = end - start;
  }
  if (!len) {
    // No characters can fit in the available space.
    // Break at the first mid-word break opportunity and let the line overflow.
    return RewindToFirstMidWordBreak(text, style, font, break_iterator,
                                     word_measurement);
  }

  FloatRect rect = font.SelectionRectForText(run, FloatPoint(), 0, 0, len);
  return RewindToMidWordBreak(word_measurement, end, rect.Width());
}

ALWAYS_INLINE bool BreakingContext::Hyphenate(
    LineLayoutText text,
    const ComputedStyle& style,
    const Font& font,
    const Hyphenation& hyphenation,
    float last_space_word_spacing,
    WordMeasurement& word_measurement) {
  unsigned start = word_measurement.start_offset;
  unsigned len = word_measurement.end_offset - start;
  if (len <= Hyphenation::kMinimumSuffixLength)
    return false;

  float hyphen_width = text.HyphenWidth(
      font, TextDirectionFromUnicode(resolver_.GetPosition().Direction()));
  float max_prefix_width = width_.AvailableWidth() - width_.CurrentWidth() -
                           hyphen_width - last_space_word_spacing;

  if (max_prefix_width <=
      font.GetFontDescription().MinimumPrefixWidthToHyphenate()) {
    return false;
  }

  TextRun run = ConstructTextRun(font, text, start, len, style);
  run.SetTabSize(!collapse_white_space_, style.GetTabSize());
  run.SetXPos(width_.CurrentWidth());
  // TODO(fserb): Check if this need to be BreakGlyphs.
  unsigned max_prefix_length = font.OffsetForPosition(
      run, max_prefix_width, OnlyFullGlyphs, DontBreakGlyphs);
  if (max_prefix_length < Hyphenation::kMinimumPrefixLength)
    return false;

  unsigned prefix_length = hyphenation.LastHyphenLocation(
      StringView(text.GetText(), start, len),
      std::min(max_prefix_length, len - Hyphenation::kMinimumSuffixLength) + 1);
  DCHECK_LE(prefix_length, max_prefix_length);
  if (!prefix_length || prefix_length < Hyphenation::kMinimumPrefixLength)
    return false;

  // TODO(kojii): getCharacterRange() measures as if the word were not broken
  // as defined in the spec, and is faster than measuring each fragment, but
  // ignores the kerning between the last letter and the hyphen.
  return RewindToMidWordBreak(
      word_measurement, start + prefix_length,
      font.GetCharacterRange(run, 0, prefix_length).Width());
}

ALWAYS_INLINE bool BreakingContext::IsBreakAtSoftHyphen() const {
  return line_break_ != resolver_.GetPosition()
             ? line_break_.PreviousInSameNode() == kSoftHyphenCharacter
             : current_.PreviousInSameNode() == kSoftHyphenCharacter;
}

static ALWAYS_INLINE bool HasVisibleText(LineLayoutText layout_text,
                                         unsigned offset) {
  return !layout_text.ContainsOnlyWhitespace(offset,
                                             layout_text.TextLength() - offset);
}

inline bool BreakingContext::HandleText(WordMeasurements& word_measurements,
                                        bool& hyphenated) {
  if (!current_.Offset())
    applied_start_width_ = false;

  LineLayoutText layout_text(current_.GetLineLayoutItem());

  // If we have left a no-wrap inline and entered an autowrap inline while
  // ignoring spaces then we need to mark the start of the autowrap inline as a
  // potential linebreak now.
  if (auto_wrap_ && !ComputedStyle::AutoWrap(last_ws_) && ignoring_spaces_) {
    width_.Commit();
    line_break_.MoveToStartOf(current_.GetLineLayoutItem());
  }

  const ComputedStyle& style = layout_text.StyleRef(line_info_.IsFirstLine());
  const Font& font = style.GetFont();

  unsigned last_space = current_.Offset();
  unsigned next_grapheme_cluster = 0;
  float word_spacing = current_style_->WordSpacing();
  float last_space_word_spacing = 0;
  float word_spacing_for_word_measurement = 0;

  float width_from_last_breaking_opportunity = width_.UncommittedWidth();
  float char_width = 0;
  // Auto-wrapping text should wrap in the middle of a word only if it could not
  // wrap before the word, which is only possible if the word is the first thing
  // on the line, that is, if |w| is zero.
  bool break_words = current_style_->BreakWords() &&
                     ((auto_wrap_ && !width_.CommittedWidth()) ||
                      curr_ws_ == EWhiteSpace::kPre);
  bool mid_word_break = false;
  bool line_break_anywhere =
      auto_wrap_ ? current_style_->GetLineBreak() == LineBreak::kAnywhere
                 : false;
  bool break_all =
      auto_wrap_ && (current_style_->WordBreak() == EWordBreak::kBreakAll ||
                     line_break_anywhere);
  bool keep_all =
      auto_wrap_ && current_style_->WordBreak() == EWordBreak::kKeepAll;
  bool prohibit_break_inside = current_style_->HasTextCombine() &&
                               layout_text.IsCombineText() &&
                               LineLayoutTextCombine(layout_text).IsCombined();

  // This is currently only used for word-break: break-all, specifically for the
  // case where we have a break opportunity within a word, then a string of non-
  // breakable content that ends up making our word wider than the current line.
  // See: fast/css3-text/css3-word-break/word-break-all-wrap-with-floats.html
  float width_measurement_at_last_break_opportunity = 0;

  Hyphenation* hyphenation = auto_wrap_ ? style.GetHyphenation() : nullptr;
  bool disable_soft_hyphen = style.GetHyphens() == Hyphens::kNone;
  float hyphen_width = 0;
  bool is_line_empty = line_info_.IsEmpty();

  if (layout_text.IsSVGInlineText()) {
    break_words = false;
    break_all = false;
    keep_all = false;
  }

  // Use LineBreakType::Normal for break-all. When a word does not fit,
  // rewindToMidWordBreak() finds the mid-word break point.
  LineBreakType line_break_type =
      keep_all ? LineBreakType::kKeepAll : LineBreakType::kNormal;
  LineBreakType line_break_type_for_rewind =
      break_all && !line_break_anywhere ? LineBreakType::kBreakAll
                                        : LineBreakType::kBreakCharacter;
  bool can_break_mid_word = break_all || break_words;
  int next_breakable_position_for_break_all = -1;

  if (layout_text.IsWordBreak()) {
    width_.Commit();
    line_break_.MoveToStartOf(current_.GetLineLayoutItem());
    DCHECK_EQ(current_.Offset(), layout_text.TextLength());
  }

  if (layout_text_info_.text_ != layout_text) {
    layout_text_info_.text_ = layout_text;
    layout_text_info_.font_ = &font;
    layout_text_info_.line_break_iterator_.ResetStringAndReleaseIterator(
        layout_text.GetText(), style.LocaleForLineBreakIterator());
  } else if (layout_text_info_.font_ != &font) {
    layout_text_info_.font_ = &font;
  }

  // Non-zero only when kerning is enabled, in which case we measure
  // words with their trailing space, then subtract its width.
  float word_trailing_space_width =
      (font.GetFontDescription().GetTypesettingFeatures() & kKerning)
          ? font.Width(ConstructTextRun(font, &kSpaceCharacter, 1, style,
                                        style.Direction())) +
                word_spacing
          : 0;

  UChar last_character = layout_text_info_.line_break_iterator_.LastCharacter();
  UChar second_to_last_character =
      layout_text_info_.line_break_iterator_.SecondToLastCharacter();
  for (; current_.Offset() < layout_text.TextLength();
       current_.FastIncrementInTextNode()) {
    previous_character_is_space_ = current_character_is_space_;
    UChar c = current_.Current();
    SetCurrentCharacterIsSpace(c);

    // A single preserved leading white-space doesn't fulfill the 'betweenWords'
    // condition, however it's indeed a soft-breaking opportunty so we may want
    // to avoid breaking in the middle of the word.
    if (at_start_ && current_character_is_space_ &&
        !previous_character_is_space_) {
      has_former_opportunity_ = !line_break_anywhere;
      break_words = false;
      can_break_mid_word = break_all;
    }

    if (!collapse_white_space_ || !current_character_is_space_) {
      line_info_.SetEmpty(false);
      width_.SetTrailingWhitespaceWidth(0);
    }

    if (c == kSoftHyphenCharacter && auto_wrap_ && !hyphen_width &&
        !disable_soft_hyphen) {
      hyphen_width = layout_text.HyphenWidth(
          font, TextDirectionFromUnicode(resolver_.GetPosition().Direction()));
      width_.AddUncommittedWidth(hyphen_width);
    }

    bool apply_word_spacing = false;

    // Determine if we should try breaking in the middle of a word.
    if (can_break_mid_word && !mid_word_break &&
        current_.Offset() >= next_grapheme_cluster) {
      mid_word_break =
          ShouldMidWordBreak(c, layout_text, font, next_grapheme_cluster,
                             char_width, width_from_last_breaking_opportunity,
                             break_all, next_breakable_position_for_break_all);
    }

    // Determine if we are in the whitespace between words.
    int next_breakable_position = current_.NextBreakablePosition();
    bool between_words =
        c == kNewlineCharacter ||
        (curr_ws_ != EWhiteSpace::kPre && !at_start_ &&
         layout_text_info_.line_break_iterator_.IsBreakable(
             current_.Offset(), next_breakable_position, line_break_type) &&
         (!disable_soft_hyphen ||
          current_.PreviousInSameNode() != kSoftHyphenCharacter));
    current_.SetNextBreakablePosition(next_breakable_position);

    // If we're in the middle of a word or at the start of a new one and can't
    // break there, then continue to the next character.
    if (!between_words && !mid_word_break) {
      if (ignoring_spaces_) {
        // Stop ignoring spaces and begin at this
        // new point.
        last_space_word_spacing = apply_word_spacing ? word_spacing : 0;
        word_spacing_for_word_measurement =
            (apply_word_spacing && word_measurements.back().width)
                ? word_spacing
                : 0;
        StopIgnoringSpaces(last_space);
      }

      PrepareForNextCharacter(layout_text, prohibit_break_inside,
                              previous_character_is_space_);
      at_start_ = false;
      NextCharacter(c, last_character, second_to_last_character);
      continue;
    }

    // If we're collapsing space and we're at a collapsible space such as a
    // space or tab, continue to the next character.
    if (ignoring_spaces_ && current_character_is_space_) {
      last_space_word_spacing = 0;
      // Just keep ignoring these spaces.
      NextCharacter(c, last_character, second_to_last_character);
      continue;
    }

    // We're in the first whitespace after a word or in whitespace that we don't
    // collapse, which means we may have a breaking opportunity here.

    // If we're here and we're collapsing space then the current character isn't
    // a form of whitespace we can collapse. Stop ignoring spaces.
    bool stopped_ignoring_spaces = false;
    if (ignoring_spaces_) {
      last_space_word_spacing = 0;
      word_spacing_for_word_measurement = 0;
      stopped_ignoring_spaces = true;
      StopIgnoringSpaces(last_space);
    }

    // Update our tally of the width since the last breakable position with the
    // width of the word we're now at the end of.
    float last_width_measurement;
    WordMeasurement& word_measurement = CalculateWordWidth(
        word_measurements, layout_text, last_space, last_width_measurement,
        word_spacing_for_word_measurement, font, word_trailing_space_width, c);
    last_width_measurement += last_space_word_spacing;
    width_.AddUncommittedWidth(last_width_measurement);

    // We keep track of the total width contributed by trailing space as we
    // often want to exclude it when determining
    // if a run fits on a line.
    if (collapse_white_space_ && previous_character_is_space_ &&
        current_character_is_space_ && last_width_measurement)
      width_.SetTrailingWhitespaceWidth(last_width_measurement);

    // If this is the end of the first word in run of text then make sure we
    // apply the width from any leading inlines.
    // For example: '<span style="margin-left: 5px;"><span style="margin-left:
    // 10px;">FirstWord</span></span>' would apply a width of 15px from the two
    // span ancestors.
    if (!applied_start_width_) {
      width_.AddUncommittedWidth(InlineLogicalWidthFromAncestorsIfNeeded(
                                     current_.GetLineLayoutItem(), true, false)
                                     .ToFloat());
      applied_start_width_ = true;
    }

    mid_word_break = false;
    if (!width_.FitsOnLine()) {
      if (hyphenation && (next_object_ || is_line_empty ||
                          HasVisibleText(layout_text, current_.Offset()))) {
        width_.AddUncommittedWidth(-word_measurement.width);
        DCHECK_EQ(last_space,
                  static_cast<unsigned>(word_measurement.start_offset));
        DCHECK_EQ(current_.Offset(),
                  static_cast<unsigned>(word_measurement.end_offset));
        if (Hyphenate(layout_text, style, font, *hyphenation,
                      last_space_word_spacing, word_measurement)) {
          width_.AddUncommittedWidth(word_measurement.width);
          hyphenated = true;
          at_end_ = true;
          return false;
        }
        width_.AddUncommittedWidth(word_measurement.width);
      }
      if (can_break_mid_word) {
        width_.AddUncommittedWidth(-word_measurement.width);
        if (RewindToMidWordBreak(layout_text, style, font,
                                 line_break_type_for_rewind,
                                 word_measurement)) {
          last_width_measurement =
              word_measurement.width + last_space_word_spacing;
          mid_word_break = true;
        }
        width_.AddUncommittedWidth(word_measurement.width);
      }
    }

    // If we haven't hit a breakable position yet and already don't fit on the
    // line try to move below any floats.
    if (!width_.CommittedWidth() && auto_wrap_ && !width_.FitsOnLine() &&
        !width_measurement_at_last_break_opportunity) {
      float available_width_before = width_.AvailableWidth();
      width_.FitBelowFloats(line_info_.IsFirstLine());
      // If availableWidth changes by moving the line below floats, needs to
      // measure midWordBreak again.
      if (mid_word_break && available_width_before != width_.AvailableWidth())
        mid_word_break = false;
    }

    // If there is a soft-break available at this whitespace position then take
    // it.
    apply_word_spacing = word_spacing && current_character_is_space_;
    if (CanBreakAtWhitespace(
            break_words, word_measurement, stopped_ignoring_spaces, char_width,
            hyphenated, disable_soft_hyphen, hyphen_width, between_words,
            mid_word_break, can_break_mid_word, previous_character_is_space_,
            last_width_measurement, layout_text, font, apply_word_spacing,
            word_spacing))
      return false;

    // If there is a hard-break available at this whitespace position then take
    // it.
    if (c == kNewlineCharacter && preserves_newline_) {
      if (!stopped_ignoring_spaces && current_.Offset())
        line_midpoint_state_.EnsureCharacterGetsLineBox(current_);
      line_break_.MoveTo(current_.GetLineLayoutItem(), current_.Offset(),
                         current_.NextBreakablePosition());
      line_break_.Increment();
      line_info_.SetPreviousLineBrokeCleanly(true);
      return true;
    }

    // Auto-wrapping text should not wrap in the middle of a word once it has
    // had an opportunity to break after a word.
    if (auto_wrap_ && between_words) {
      width_.Commit();
      width_from_last_breaking_opportunity = 0;
      line_break_.MoveTo(current_.GetLineLayoutItem(), current_.Offset(),
                         current_.NextBreakablePosition());
      has_former_opportunity_ = !line_break_anywhere;
      break_words = false;
      can_break_mid_word = break_all;
      width_measurement_at_last_break_opportunity = last_width_measurement;
    }

    if (between_words) {
      last_space_word_spacing = apply_word_spacing ? word_spacing : 0;
      word_spacing_for_word_measurement =
          (apply_word_spacing && word_measurement.width) ? word_spacing : 0;
      last_space = current_.Offset();
    }

    // If we encounter a newline, or if we encounter a second space, we need to
    // go ahead and break up this run and enter a mode where we start collapsing
    // spaces.
    if (!ignoring_spaces_ && current_style_->CollapseWhiteSpace()) {
      if (current_character_is_space_ && previous_character_is_space_) {
        ignoring_spaces_ = true;

        // We just entered a mode where we are ignoring spaces. Create a
        // midpoint to terminate the run before the second space.
        line_midpoint_state_.StartIgnoringSpaces(start_of_ignored_spaces_);
        trailing_objects_.UpdateMidpointsForTrailingObjects(
            line_midpoint_state_, InlineIterator(),
            TrailingObjects::kDoNotCollapseFirstSpace);
      }
    }

    PrepareForNextCharacter(layout_text, prohibit_break_inside,
                            previous_character_is_space_);
    at_start_ = false;
    is_line_empty = line_info_.IsEmpty();
    NextCharacter(c, last_character, second_to_last_character);
  }

  layout_text_info_.line_break_iterator_.SetPriorContext(
      last_character, second_to_last_character);

  word_measurements.Grow(word_measurements.size() + 1);
  WordMeasurement& word_measurement = word_measurements.back();
  word_measurement.layout_text = layout_text;

  // IMPORTANT: current.offset() is > layoutText.textLength() here!
  float last_width_measurement = 0;
  word_measurement.start_offset = last_space;
  word_measurement.end_offset = current_.Offset();
  if (!ignoring_spaces_) {
    last_width_measurement = TextWidth(
        layout_text, last_space, current_.Offset() - last_space, font,
        width_.CurrentWidth(), collapse_white_space_,
        &word_measurement.fallback_fonts, &word_measurement.glyph_bounds);
    word_measurement.width =
        last_width_measurement + word_spacing_for_word_measurement;
    word_measurement.glyph_bounds.Move(word_spacing_for_word_measurement, 0);
  }
  last_width_measurement += last_space_word_spacing;

  LayoutUnit additional_width_from_ancestors =
      InlineLogicalWidthFromAncestorsIfNeeded(current_.GetLineLayoutItem(),
                                              !applied_start_width_,
                                              include_end_width_);
  width_.AddUncommittedWidth(last_width_measurement +
                             additional_width_from_ancestors);

  if (collapse_white_space_ && current_character_is_space_ &&
      last_width_measurement)
    width_.SetTrailingWhitespaceWidth(last_width_measurement +
                                      additional_width_from_ancestors);

  include_end_width_ = false;

  if (!ignoring_spaces_ && !width_.FitsOnLine()) {
    if (hyphenation && (next_object_ || is_line_empty)) {
      width_.AddUncommittedWidth(-word_measurement.width);
      DCHECK_EQ(last_space,
                static_cast<unsigned>(word_measurement.start_offset));
      DCHECK_EQ(current_.Offset(),
                static_cast<unsigned>(word_measurement.end_offset));
      if (Hyphenate(layout_text, style, font, *hyphenation,
                    last_space_word_spacing, word_measurement)) {
        hyphenated = true;
        at_end_ = true;
      }
      width_.AddUncommittedWidth(word_measurement.width);
    }
    if (!hyphenated && IsBreakAtSoftHyphen() && !disable_soft_hyphen) {
      hyphenated = true;
      at_end_ = true;
    }
    if (!hyphenated && can_break_mid_word) {
      width_.AddUncommittedWidth(-word_measurement.width);
      if (RewindToMidWordBreak(layout_text, style, font,
                               line_break_type_for_rewind, word_measurement)) {
        width_.AddUncommittedWidth(word_measurement.width);
        width_.Commit();
        at_end_ = true;
      } else {
        width_.AddUncommittedWidth(word_measurement.width);
        if (width_.CommittedWidth())
          at_end_ = true;
      }
    }
  }
  return false;
}

inline void BreakingContext::PrepareForNextCharacter(
    const LineLayoutText& layout_text,
    bool& prohibit_break_inside,
    bool previous_character_is_space) {
  if (layout_text.IsSVGInlineText() && current_.Offset()) {
    // Force creation of new InlineBoxes for each absolute positioned character
    // (those that start new text chunks).
    if (LineLayoutSVGInlineText(layout_text)
            .CharacterStartsNewTextChunk(current_.Offset()))
      line_midpoint_state_.EnsureCharacterGetsLineBox(current_);
  }
  if (prohibit_break_inside) {
    current_.SetNextBreakablePosition(layout_text.TextLength());
    prohibit_break_inside = false;
  }
  if (current_character_is_space_ && !previous_character_is_space) {
    start_of_ignored_spaces_.SetLineLayoutItem(current_.GetLineLayoutItem());
    start_of_ignored_spaces_.SetOffset(current_.Offset());
  }
  if (!current_character_is_space_ && previous_character_is_space) {
    if (auto_wrap_ && current_style_->BreakOnlyAfterWhiteSpace()) {
      line_break_.MoveTo(current_.GetLineLayoutItem(), current_.Offset(),
                         current_.NextBreakablePosition());
    }
  }
  if (collapse_white_space_ && current_character_is_space_ && !ignoring_spaces_)
    trailing_objects_.SetTrailingWhitespace(
        LineLayoutText(current_.GetLineLayoutItem()));
  else if (!current_style_->CollapseWhiteSpace() ||
           !current_character_is_space_)
    trailing_objects_.Clear();
}

inline void BreakingContext::StopIgnoringSpaces(unsigned& last_space) {
  ignoring_spaces_ = false;
  // e.g., "Foo    goo", don't add in any of the ignored spaces.
  last_space = current_.Offset();
  line_midpoint_state_.StopIgnoringSpaces(
      InlineIterator(nullptr, current_.GetLineLayoutItem(), current_.Offset()));
}

inline WordMeasurement& BreakingContext::CalculateWordWidth(
    WordMeasurements& word_measurements,
    LineLayoutText& layout_text,
    unsigned last_space,
    float& last_width_measurement,
    float word_spacing_for_word_measurement,
    const Font& font,
    float word_trailing_space_width,
    UChar c) {
  word_measurements.Grow(word_measurements.size() + 1);
  WordMeasurement& word_measurement = word_measurements.back();
  word_measurement.layout_text = layout_text;
  word_measurement.end_offset = current_.Offset();
  word_measurement.start_offset = last_space;

  if (word_trailing_space_width && c == kSpaceCharacter)
    last_width_measurement =
        TextWidth(layout_text, last_space, current_.Offset() + 1 - last_space,
                  font, width_.CurrentWidth(), collapse_white_space_,
                  &word_measurement.fallback_fonts,
                  &word_measurement.glyph_bounds) -
        word_trailing_space_width;
  else
    last_width_measurement = TextWidth(
        layout_text, last_space, current_.Offset() - last_space, font,
        width_.CurrentWidth(), collapse_white_space_,
        &word_measurement.fallback_fonts, &word_measurement.glyph_bounds);

  word_measurement.width =
      last_width_measurement + word_spacing_for_word_measurement;
  word_measurement.glyph_bounds.Move(word_spacing_for_word_measurement, 0);
  return word_measurement;
}

inline void BreakingContext::TrailingSpacesHang(bool can_break_mid_word) {
  DCHECK(curr_ws_ == EWhiteSpace::kBreakSpaces);
  // Avoid breaking before the first white-space after a word if there is a
  // breaking opportunity before.
  if (has_former_opportunity_ && !previous_character_is_space_)
    return;

  line_break_.MoveTo(current_.GetLineLayoutItem(), current_.Offset(),
                     current_.NextBreakablePosition());

  // Avoid breaking before the first white-space after a word, unless
  // overflow-wrap or word-break allow to.
  if (!previous_character_is_space_ && !can_break_mid_word)
    line_break_.Increment();
}

inline bool BreakingContext::TrailingSpaceExceedsAvailableWidth(
    bool can_break_mid_word,
    const LineLayoutText& layout_text,
    WordMeasurement& word_measurement,
    bool apply_word_spacing,
    bool word_spacing,
    const Font& font) {
  // If we break only after white-space, consider the current character
  // as candidate width for this line.
  if (width_.FitsOnLine() && current_character_is_space_ &&
      current_style_->BreakOnlyAfterWhiteSpace()) {
    float char_width = TextWidth(layout_text, current_.Offset(), 1, font,
                                 width_.CurrentWidth(), collapse_white_space_,
                                 &word_measurement.fallback_fonts,
                                 &word_measurement.glyph_bounds) +
                       (apply_word_spacing ? word_spacing : 0);
    // Check if line is too big even without the extra space
    // at the end of the line. If it is not, do nothing.
    // If the line needs the extra whitespace to be too long,
    // then move the line break to the space and skip all
    // additional whitespace.
    if (!width_.FitsOnLine(char_width)) {
      if (curr_ws_ == EWhiteSpace::kBreakSpaces) {
        TrailingSpacesHang(can_break_mid_word);
        return true;
      }
      line_break_.MoveTo(current_.GetLineLayoutItem(), current_.Offset(),
                         current_.NextBreakablePosition());

      SkipTrailingWhitespace(line_break_, line_info_);
      return true;
    }
  }
  return false;
}

inline bool BreakingContext::CanBreakAtWhitespace(
    bool break_words,
    WordMeasurement& word_measurement,
    bool stopped_ignoring_spaces,
    float char_width,
    bool& hyphenated,
    bool disable_soft_hyphen,
    float& hyphen_width,
    bool between_words,
    bool mid_word_break,
    bool can_break_mid_word,
    bool previous_character_is_space,
    float last_width_measurement,
    const LineLayoutText& layout_text,
    const Font& font,
    bool apply_word_spacing,
    float word_spacing) {
  if (!auto_wrap_ && !break_words)
    return false;

  // If we break only after white-space, consider the current character
  // as candidate width for this line.
  if (mid_word_break ||
      TrailingSpaceExceedsAvailableWidth(can_break_mid_word, layout_text,
                                         word_measurement, apply_word_spacing,
                                         word_spacing, font) ||
      !width_.FitsOnLine()) {
    if (line_break_.AtTextParagraphSeparator()) {
      if (!stopped_ignoring_spaces && current_.Offset() > 0)
        line_midpoint_state_.EnsureCharacterGetsLineBox(current_);
      line_break_.Increment();
      line_info_.SetPreviousLineBrokeCleanly(true);
      word_measurement.end_offset = line_break_.Offset();
    }
    if (IsBreakAtSoftHyphen() && !disable_soft_hyphen)
      hyphenated = true;
    if (line_break_.Offset() &&
        line_break_.Offset() != (unsigned)word_measurement.end_offset &&
        !word_measurement.width) {
      if (char_width) {
        word_measurement.end_offset = line_break_.Offset();
        word_measurement.width = char_width;
      }
    }
    // Didn't fit. Jump to the end unless there's still an opportunity to
    // collapse whitespace.
    if (ignoring_spaces_ || !collapse_white_space_ ||
        !current_character_is_space_ || !previous_character_is_space) {
      at_end_ = true;
      return true;
    }
  } else {
    if (!between_words || (mid_word_break && !auto_wrap_))
      width_.AddUncommittedWidth(-last_width_measurement);
    if (hyphen_width) {
      // Subtract the width of the soft hyphen out since we fit on a line.
      width_.AddUncommittedWidth(-hyphen_width);
      hyphen_width = 0;
    }
  }
  return false;
}

inline void BreakingContext::CommitAndUpdateLineBreakIfNeeded() {
  bool check_for_break = auto_wrap_;
  if (width_.CommittedWidth() && !width_.FitsOnLine() &&
      line_break_.GetLineLayoutItem() && curr_ws_ == EWhiteSpace::kNowrap) {
    // If this nowrap item fits but its trailing spaces does not, and if the
    // next item is auto-wrap, break before the next item.
    // TODO(kojii): This case should be handled when we read next item.
    if (width_.FitsOnLine(0, kExcludeWhitespace) &&
        (!next_object_ || next_object_.StyleRef().AutoWrap())) {
      width_.Commit();
      line_break_.MoveToStartOf(next_object_);
    }
    check_for_break = true;
  } else if (next_object_ && current_.GetLineLayoutItem().IsText() &&
             next_object_.IsText() && !next_object_.IsBR() &&
             (auto_wrap_ || next_object_.StyleRef().AutoWrap())) {
    if (auto_wrap_ && current_character_is_space_) {
      check_for_break = true;
    } else {
      LineLayoutText next_text(next_object_);
      if (next_text.TextLength()) {
        UChar c = next_text.CharacterAt(0);
        // If the next item on the line is text, and if we did not end with
        // a space, then the next text run continues our word (and so it needs
        // to keep adding to the uncommitted width. Just update and continue.
        check_for_break =
            !current_character_is_space_ &&
            (c == kSpaceCharacter || c == kTabulationCharacter ||
             (c == kNewlineCharacter && !next_object_.PreservesNewline()));
      } else if (next_text.IsWordBreak()) {
        check_for_break = true;
      }

      if (!width_.FitsOnLine() && !width_.CommittedWidth())
        width_.FitBelowFloats(line_info_.IsFirstLine());

      bool can_place_on_line =
          width_.FitsOnLine() || !auto_wrap_was_ever_true_on_line_;
      if (can_place_on_line && check_for_break) {
        width_.Commit();
        line_break_.MoveToStartOf(next_object_);
      }
    }
  }

  if (check_for_break && !width_.FitsOnLine()) {
    // if we have floats, try to get below them.
    if (current_character_is_space_ && !ignoring_spaces_ &&
        current_style_->CollapseWhiteSpace())
      trailing_objects_.Clear();

    // If we are going to break before a float on an object that is just
    // collapsible white space then make sure the line break is moved to
    // the float.
    if (width_.FitsOnLine(0, kExcludeWhitespace) && next_object_ &&
        next_object_.IsFloating() && current_.GetLineLayoutItem().IsText() &&
        LineLayoutText(current_.GetLineLayoutItem())
            .IsAllCollapsibleWhitespace()) {
      width_.Commit();
      line_break_.MoveToStartOf(next_object_);
    }

    if (width_.CommittedWidth()) {
      at_end_ = true;
      return;
    }

    width_.FitBelowFloats(line_info_.IsFirstLine());

    // |width| may have been adjusted because we got shoved down past a float
    // (thus giving us more room), so we need to retest, and only jump to the
    // the end label if we still don't fit on the line. -dwh
    if (!width_.FitsOnLine()) {
      at_end_ = true;
      return;
    }
  } else if (block_style_->AutoWrap() && !width_.FitsOnLine() &&
             !width_.CommittedWidth()) {
    // If the container autowraps but the current child does not then we still
    // need to ensure that it wraps and moves below any floats.
    width_.FitBelowFloats(line_info_.IsFirstLine());
  }

  if (!current_.GetLineLayoutItem().IsFloatingOrOutOfFlowPositioned()) {
    last_object_ = current_.GetLineLayoutItem();
    if (last_object_.IsAtomicInlineLevel() && auto_wrap_ &&
        (!last_object_.IsListMarker() ||
         LineLayoutListMarker(last_object_).IsInside()) &&
        !last_object_.IsRubyRun()) {
      width_.Commit();
      line_break_.MoveToStartOf(next_object_);
    }
  }
}

inline IndentTextOrNot RequiresIndent(bool is_first_line,
                                      bool is_after_hard_line_break,
                                      const ComputedStyle& style) {
  IndentTextOrNot indent_text = kDoNotIndentText;
  if (is_first_line ||
      (is_after_hard_line_break &&
       style.GetTextIndentLine() != TextIndentLine::kFirstLine))
    indent_text = kIndentText;

  if (style.GetTextIndentType() == TextIndentType::kHanging)
    indent_text = indent_text == kIndentText ? kDoNotIndentText : kIndentText;

  return indent_text;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_BREAKING_CONTEXT_INLINE_HEADERS_H_
