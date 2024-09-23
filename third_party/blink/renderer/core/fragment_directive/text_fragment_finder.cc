// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_finder.h"

#include <memory>

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/finder/async_find_buffer.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"
#include "third_party/blink/renderer/core/editing/finder/sync_find_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/backwards_character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// TODO(crbug/924965): Determine how this should check node boundaries. This
// treats node boundaries as word boundaries, for example "o" is a whole word
// match in "f<i>o</i>o".
// Determines whether the |start| and/or |end| positions of |range| are on a
// word boundaries.
bool IsWordBounded(EphemeralRangeInFlatTree range, bool start, bool end) {
  if (!start && !end)
    return true;

  wtf_size_t start_position = range.StartPosition().OffsetInContainerNode();

  if (start_position != 0 && start) {
    String start_text = range.StartPosition().AnchorNode()->textContent();
    start_text.Ensure16Bit();
    wtf_size_t word_start =
        FindWordStartBoundary(start_text.Span16(), start_position);
    if (word_start != start_position)
      return false;
  }

  wtf_size_t end_position = range.EndPosition().OffsetInContainerNode();
  String end_text = range.EndPosition().AnchorNode()->textContent();

  if (end_position != end_text.length() && end) {
    end_text.Ensure16Bit();
    // We expect end_position to be a word boundary, and FindWordEndBoundary
    // finds the next word boundary, so start from end_position - 1.
    wtf_size_t word_end =
        FindWordEndBoundary(end_text.Span16(), end_position - 1);
    if (word_end != end_position)
      return false;
  }

  return true;
}

PositionInFlatTree FirstWordBoundaryAfter(PositionInFlatTree position) {
  wtf_size_t offset = position.OffsetInContainerNode();
  String text = position.AnchorNode()->textContent();

  if (offset == text.length()) {
    PositionIteratorInFlatTree itr(position);
    if (itr.AtEnd())
      return position;

    itr.Increment();
    return itr.ComputePosition();
  }

  text.Ensure16Bit();
  wtf_size_t word_end = FindWordEndBoundary(text.Span16(), offset);

  PositionInFlatTree end_pos(position.AnchorNode(), word_end);
  PositionIteratorInFlatTree itr(end_pos);
  itr.Increment();
  if (itr.AtEnd())
    return end_pos;
  return itr.ComputePosition();
}

}  // namespace

// static
PositionInFlatTree TextFragmentFinder::NextTextPosition(
    PositionInFlatTree position,
    PositionInFlatTree end_position) {
  const TextIteratorBehavior options =
      TextIteratorBehavior::Builder().SetEmitsSpaceForNbsp(true).Build();
  CharacterIteratorInFlatTree char_it(position, end_position, options);
  for (; char_it.length(); char_it.Advance(1)) {
    if (!IsSpaceOrNewline(char_it.CharacterAt(0)))
      return char_it.StartPosition();
  }

  return end_position;
}

// static
PositionInFlatTree TextFragmentFinder::PreviousTextPosition(
    PositionInFlatTree position,
    PositionInFlatTree max_position) {
  const TextIteratorBehavior options =
      TextIteratorBehavior::Builder().SetEmitsSpaceForNbsp(true).Build();
  BackwardsCharacterIteratorInFlatTree char_it(
      EphemeralRangeInFlatTree(max_position, position), options);

  for (; char_it.length(); char_it.Advance(1)) {
    if (!IsSpaceOrNewline(char_it.CharacterAt(0)))
      return char_it.EndPosition();
  }

  return max_position;
}

void TextFragmentFinder::OnFindMatchInRangeComplete(
    String search_text,
    RangeInFlatTree* search_range,
    bool word_start_bounded,
    bool word_end_bounded,
    const EphemeralRangeInFlatTree& match) {
  // If any of our ranges became invalid, stop the search.
  if (!HasValidRanges()) {
    potential_match_.Clear();
    first_match_.Clear();
    OnMatchComplete();
    return;
  }

  if (match.IsNull() ||
      IsWordBounded(match, word_start_bounded, word_end_bounded)) {
    switch (step_) {
      case kMatchPrefix:
        OnPrefixMatchComplete(match);
        break;
      case kMatchTextStart:
        OnTextStartMatchComplete(match);
        break;
      case kMatchTextEnd:
        OnTextEndMatchComplete(match);
        break;
      case kMatchSuffix:
        OnSuffixMatchComplete(match);
        break;
    }
    return;
  }
  search_range->SetStart(match.EndPosition());
  FindMatchInRange(search_text, search_range, word_start_bounded,
                   word_end_bounded);
}

void TextFragmentFinder::FindMatchInRange(String search_text,
                                          RangeInFlatTree* search_range,
                                          bool word_start_bounded,
                                          bool word_end_bounded) {
  find_buffer_runner_->FindMatchInRange(
      search_range, search_text, FindOptions().SetCaseInsensitive(true),
      WTF::BindOnce(&TextFragmentFinder::OnFindMatchInRangeComplete,
                    WrapWeakPersistent(this), search_text,
                    WrapWeakPersistent(search_range), word_start_bounded,
                    word_end_bounded));
}

void TextFragmentFinder::FindPrefix() {
  search_range_->SetStart(match_range_->StartPosition());
  if (search_range_->IsCollapsed()) {
    OnMatchComplete();
    return;
  }

  if (selector_.Prefix().empty()) {
    GoToStep(kMatchTextStart);
    return;
  }

  FindMatchInRange(selector_.Prefix(), search_range_,
                   /*word_start_bounded=*/true,
                   /*word_end_bounded=*/false);
}

void TextFragmentFinder::OnPrefixMatchComplete(
    EphemeralRangeInFlatTree prefix_match) {
  // No prefix_match in remaining range
  if (prefix_match.IsNull()) {
    OnMatchComplete();
    return;
  }

  // If we iterate again, start searching from the first boundary after the
  // prefix start (since prefix must start at a boundary). Note, we don't
  // advance to the prefix end; this is done since, if this prefix isn't
  // the one we're looking for, the next occurrence might be overlapping
  // with the current one. e.g. If |prefix| is "a a" and our search range
  // currently starts with "a a a b...", the next iteration should start at
  // the second a which is part of the current |prefix_match|.
  match_range_->SetStart(FirstWordBoundaryAfter(prefix_match.StartPosition()));
  SetPrefixMatch(prefix_match);
  GoToStep(kMatchTextStart);
  return;
}

void TextFragmentFinder::FindTextStart() {
  DCHECK(!selector_.Start().empty());

  // The match text need not be bounded at the end. If this is an exact
  // match (i.e. no |end_text|) and we have a suffix then the suffix will
  // be required to end on the word boundary instead. Since we have a
  // prefix, we don't need the match to be word bounded. See
  // https://github.com/WICG/scroll-to-text-fragment/issues/137 for
  // details.
  const bool end_at_word_boundary =
      !selector_.End().empty() || selector_.Suffix().empty();
  if (prefix_match_) {
    search_range_->SetStart(NextTextPosition(prefix_match_->EndPosition(),
                                             match_range_->EndPosition()));
    FindMatchInRange(selector_.Start(), search_range_,
                     /*word_start_bounded=*/false, end_at_word_boundary);
  } else {
    FindMatchInRange(selector_.Start(), search_range_,
                     /*word_start_bounded=*/true, end_at_word_boundary);
  }
}

void TextFragmentFinder::OnTextStartMatchComplete(
    EphemeralRangeInFlatTree potential_match) {
  if (prefix_match_) {
    PositionInFlatTree next_position_after_prefix = NextTextPosition(
        prefix_match_->EndPosition(), match_range_->EndPosition());
    // We found a potential match but it didn't immediately follow the prefix.
    if (!potential_match.IsNull() &&
        potential_match.StartPosition() != next_position_after_prefix) {
      potential_match_.Clear();
      GoToStep(kMatchPrefix);
      return;
    }
  }

  // No start_text match after current prefix_match
  if (potential_match.IsNull()) {
    OnMatchComplete();
    return;
  }
  if (!prefix_match_) {
    match_range_->SetStart(
        FirstWordBoundaryAfter(potential_match.StartPosition()));
  }
  if (!range_end_search_start_) {
    range_end_search_start_ = MakeGarbageCollected<RelocatablePosition>(
        ToPositionInDOMTree(potential_match.EndPosition()));
  } else {
    range_end_search_start_->SetPosition(
        ToPositionInDOMTree(potential_match.EndPosition()));
  }
  SetPotentialMatch(potential_match);
  GoToStep(kMatchTextEnd);
}

void TextFragmentFinder::FindTextEnd() {
  // If we've gotten here, we've found a |prefix| (if one was specified)
  // that's followed by the |start_text|. We'll now try to expand that into
  // a range match if |end_text| is specified.
  if (!selector_.End().empty()) {
    search_range_->SetStart(
        ToPositionInFlatTree(range_end_search_start_->GetPosition()));
    const bool end_at_word_boundary = selector_.Suffix().empty();

    FindMatchInRange(selector_.End(), search_range_,
                     /*word_start_bounded=*/true, end_at_word_boundary);
  } else {
    GoToStep(kMatchSuffix);
  }
}

void TextFragmentFinder::OnTextEndMatchComplete(
    EphemeralRangeInFlatTree text_end_match) {
  if (text_end_match.IsNull()) {
    potential_match_.Clear();
    OnMatchComplete();
    return;
  }

  potential_match_->SetEnd(text_end_match.EndPosition());
  GoToStep(kMatchSuffix);
}

void TextFragmentFinder::FindSuffix() {
  DCHECK(!potential_match_->IsNull());

  if (selector_.Suffix().empty()) {
    OnMatchComplete();
    return;
  }

  // Now we just have to ensure the match is followed by the |suffix|.
  search_range_->SetStart(NextTextPosition(potential_match_->EndPosition(),
                                           match_range_->EndPosition()));
  FindMatchInRange(selector_.Suffix(), search_range_,
                   /*word_start_bounded=*/false, /*word_end_bounded=*/true);
}

void TextFragmentFinder::OnSuffixMatchComplete(
    EphemeralRangeInFlatTree suffix_match) {
  // If no suffix appears in what follows the match, there's no way we can
  // possibly satisfy the constraints so bail.
  if (suffix_match.IsNull()) {
    potential_match_.Clear();
    OnMatchComplete();
    return;
  }

  PositionInFlatTree next_position_after_match = NextTextPosition(
      potential_match_->EndPosition(), match_range_->EndPosition());
  if (suffix_match.StartPosition() == next_position_after_match) {
    OnMatchComplete();
    return;
  }

  // If this is an exact match(e.g. |end_text| is not specified), and we
  // didn't match on suffix, continue searching for a new potential_match
  // from it's start.
  if (selector_.End().empty()) {
    potential_match_.Clear();
    GoToStep(kMatchPrefix);
    return;
  }

  // If this is a range match(e.g. |end_text| is specified), it is possible
  // that we found the correct range start, but not the correct range end.
  // Continue searching for it, without restarting the range start search.
  range_end_search_start_->SetPosition(
      ToPositionInDOMTree(potential_match_->EndPosition()));
  GoToStep(kMatchTextEnd);
}

void TextFragmentFinder::GoToStep(SelectorMatchStep step) {
  step_ = step;
  switch (step_) {
    case kMatchPrefix:
      FindPrefix();
      break;
    case kMatchTextStart:
      FindTextStart();
      break;
    case kMatchTextEnd:
      FindTextEnd();
      break;
    case kMatchSuffix:
      FindSuffix();
      break;
  }
}

// static
bool TextFragmentFinder::IsInSameUninterruptedBlock(
    const PositionInFlatTree& start,
    const PositionInFlatTree& end) {
  Node* start_node = start.ComputeContainerNode();
  Node* end_node = end.ComputeContainerNode();
  if (!start_node || !start_node->GetLayoutObject() || !end_node ||
      !end_node->GetLayoutObject()) {
    return true;
  }
  return FindBuffer::IsInSameUninterruptedBlock(*start_node, *end_node);
}

TextFragmentFinder::TextFragmentFinder(Client& client,
                                       const TextFragmentSelector& selector,
                                       Document* document,
                                       FindBufferRunnerType runner_type)
    : client_(client), selector_(selector), document_(document) {
  DCHECK(!selector_.Start().empty());
  DCHECK(selector_.Type() != TextFragmentSelector::SelectorType::kInvalid);
  if (runner_type == TextFragmentFinder::FindBufferRunnerType::kAsynchronous) {
    find_buffer_runner_ = MakeGarbageCollected<AsyncFindBuffer>();
  } else {
    find_buffer_runner_ = MakeGarbageCollected<SyncFindBuffer>();
  }
}

void TextFragmentFinder::Cancel() {
  if (find_buffer_runner_ && find_buffer_runner_->IsActive())
    find_buffer_runner_->Cancel();
}

void TextFragmentFinder::FindMatch() {
  Cancel();

  auto forced_lock_scope =
      document_->GetDisplayLockDocumentState().GetScopedForceActivatableLocks();
  document_->UpdateStyleAndLayout(DocumentUpdateReason::kFindInPage);

  first_match_.Clear();
  FindMatchFromPosition(PositionInFlatTree::FirstPositionInNode(*document_));
}

void TextFragmentFinder::FindMatchFromPosition(
    PositionInFlatTree search_start) {
  PositionInFlatTree search_end;
  if (document_->documentElement() &&
      document_->documentElement()->lastChild()) {
    search_end = PositionInFlatTree::AfterNode(
        *document_->documentElement()->lastChild());
  } else {
    search_end = PositionInFlatTree::LastPositionInNode(*document_);
  }
  search_range_ =
      MakeGarbageCollected<RangeInFlatTree>(search_start, search_end);
  match_range_ =
      MakeGarbageCollected<RangeInFlatTree>(search_start, search_end);
  potential_match_.Clear();
  prefix_match_.Clear();
  GoToStep(kMatchPrefix);
}

void TextFragmentFinder::OnMatchComplete() {
  if (!potential_match_ && !first_match_) {
    client_.NoMatchFound();
  } else if (potential_match_ && !first_match_) {
    // Continue searching to see if we have an ambiguous selector.
    // TODO(crbug.com/919204): This is temporary and only for measuring
    // ambiguous matching during prototyping.
    first_match_ = potential_match_;
    FindMatchFromPosition(first_match_->EndPosition());
  } else {
    EphemeralRangeInFlatTree potential_match = first_match_->ToEphemeralRange();
    client_.DidFindMatch(*first_match_, !potential_match_);
  }
}

void TextFragmentFinder::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(range_end_search_start_);
  visitor->Trace(potential_match_);
  visitor->Trace(prefix_match_);
  visitor->Trace(first_match_);
  visitor->Trace(search_range_);
  visitor->Trace(match_range_);
  visitor->Trace(find_buffer_runner_);
}

void TextFragmentFinder::SetPotentialMatch(EphemeralRangeInFlatTree range) {
  if (potential_match_) {
    potential_match_->SetStart(range.StartPosition());
    potential_match_->SetEnd(range.EndPosition());
  } else {
    potential_match_ = MakeGarbageCollected<RangeInFlatTree>(
        range.StartPosition(), range.EndPosition());
  }
}

void TextFragmentFinder::SetPrefixMatch(EphemeralRangeInFlatTree range) {
  if (prefix_match_) {
    prefix_match_->SetStart(range.StartPosition());
    prefix_match_->SetEnd(range.EndPosition());
  } else {
    prefix_match_ = MakeGarbageCollected<RangeInFlatTree>(range.StartPosition(),
                                                          range.EndPosition());
  }
}

bool TextFragmentFinder::HasValidRanges() {
  return !((prefix_match_ &&
            (prefix_match_->IsNull() || !prefix_match_->IsConnected())) ||
           (potential_match_ &&
            (potential_match_->IsNull() || !potential_match_->IsConnected())) ||
           (search_range_ &&
            (search_range_->IsNull() || !search_range_->IsConnected())) ||
           (match_range_ &&
            (match_range_->IsNull() || !match_range_->IsConnected())));
}

}  // namespace blink
