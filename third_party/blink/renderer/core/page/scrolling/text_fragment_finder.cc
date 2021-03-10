// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"

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
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
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
    wtf_size_t word_start = FindWordStartBoundary(
        start_text.Characters16(), start_text.length(), start_position);
    if (word_start != start_position)
      return false;
  }

  wtf_size_t end_position = range.EndPosition().OffsetInContainerNode();
  String end_text = range.EndPosition().AnchorNode()->textContent();

  if (end_position != end_text.length() && end) {
    end_text.Ensure16Bit();
    // We expect end_position to be a word boundary, and FindWordEndBoundary
    // finds the next word boundary, so start from end_position - 1.
    wtf_size_t word_end = FindWordEndBoundary(
        end_text.Characters16(), end_text.length(), end_position - 1);
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
  wtf_size_t word_end =
      FindWordEndBoundary(text.Characters16(), text.length(), offset);

  PositionInFlatTree end_pos(position.AnchorNode(), word_end);
  PositionIteratorInFlatTree itr(end_pos);
  if (itr.AtEnd())
    return end_pos;

  itr.Increment();
  return itr.ComputePosition();
}

PositionInFlatTree NextTextPosition(PositionInFlatTree position,
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

bool ContainedByListItem(const EphemeralRangeInFlatTree& range) {
  Node* node = range.CommonAncestorContainer();
  while (node) {
    if (ListItemOrdinal::IsListItem(*node)) {
      return true;
    }
    node = node->parentNode();
  }
  return false;
}

bool ContainedByTableCell(const EphemeralRangeInFlatTree& range) {
  Node* node = range.CommonAncestorContainer();
  while (node) {
    if (IsTableCell(node)) {
      return true;
    }
    node = node->parentNode();
  }
  return false;
}

}  // namespace

void TextFragmentFinder::OnFindMatchInRangeComplete(
    String search_text,
    Range* search_range,
    bool word_start_bounded,
    bool word_end_bounded,
    const EphemeralRangeInFlatTree& match) {
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

  search_range->setStart(ToPositionInDOMTree(match.EndPosition()));
  FindMatchInRange(search_text, search_range, word_start_bounded,
                   word_end_bounded);
}

void TextFragmentFinder::FindMatchInRange(String search_text,
                                          Range* search_range,
                                          bool word_start_bounded,
                                          bool word_end_bounded) {
  find_buffer_runner_->FindMatchInRange(
      search_range, search_text, kCaseInsensitive,
      WTF::Bind(&TextFragmentFinder::OnFindMatchInRangeComplete,
                WrapWeakPersistent(this), search_text,
                WrapWeakPersistent(search_range), word_start_bounded,
                word_end_bounded));
}

void TextFragmentFinder::FindPrefix() {
  search_range_->setStart(match_range_->StartPosition());
  if (search_range_->collapsed()) {
    OnMatchComplete();
    return;
  }

  if (selector_.Prefix().IsEmpty()) {
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
  match_range_->setStart(ToPositionInDOMTree(
      FirstWordBoundaryAfter(prefix_match.StartPosition())));
  SetPrefixMatch(prefix_match);
  GoToStep(kMatchTextStart);
  return;
}

void TextFragmentFinder::FindTextStart() {
  DCHECK(!selector_.Start().IsEmpty());

  // The match text need not be bounded at the end. If this is an exact
  // match (i.e. no |end_text|) and we have a suffix then the suffix will
  // be required to end on the word boundary instead. Since we have a
  // prefix, we don't need the match to be word bounded. See
  // https://github.com/WICG/scroll-to-text-fragment/issues/137 for
  // details.
  const bool end_at_word_boundary =
      !selector_.End().IsEmpty() || selector_.Suffix().IsEmpty();
  EphemeralRangeInFlatTree prefix_match(prefix_match_);
  EphemeralRangeInFlatTree potential_match;
  if (prefix_match.IsNotNull()) {
    search_range_->setStart(ToPositionInDOMTree(
        NextTextPosition(prefix_match.EndPosition(),
                         ToPositionInFlatTree(match_range_->EndPosition()))));
    FindMatchInRange(selector_.Start(), search_range_,
                     /*word_start_bounded=*/false, end_at_word_boundary);
  } else {
    FindMatchInRange(selector_.Start(), search_range_,
                     /*word_start_bounded=*/true, end_at_word_boundary);
  }
}

void TextFragmentFinder::OnTextStartMatchComplete(
    EphemeralRangeInFlatTree potential_match) {
  EphemeralRangeInFlatTree prefix_match(prefix_match_);
  if (prefix_match.IsNotNull()) {
    EphemeralRangeInFlatTree match_range(
        NextTextPosition(prefix_match.EndPosition(),
                         ToPositionInFlatTree(match_range_->EndPosition())),
        ToPositionInFlatTree(match_range_->EndPosition()));
    // We found a potential match but it didn't immediately follow the prefix.
    if (!potential_match.IsNull() &&
        potential_match.StartPosition() != match_range.StartPosition()) {
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
  if (prefix_match.IsNull()) {
    match_range_->setStart(ToPositionInDOMTree(
        FirstWordBoundaryAfter(potential_match.StartPosition())));
  }
  range_end_search_start_ = potential_match.EndPosition();
  SetPotentialMatch(potential_match);
  GoToStep(kMatchTextEnd);
}

void TextFragmentFinder::FindTextEnd() {
  // If we've gotten here, we've found a |prefix| (if one was specified)
  // that's followed by the |start_text|. We'll now try to expand that into
  // a range match if |end_text| is specified.
  if (!selector_.End().IsEmpty()) {
    search_range_->setStart(ToPositionInDOMTree(range_end_search_start_));
    const bool end_at_word_boundary = selector_.Suffix().IsEmpty();

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

  EphemeralRangeInFlatTree potential_match(potential_match_);
  SetPotentialMatch(EphemeralRangeInFlatTree(potential_match.StartPosition(),
                                             text_end_match.EndPosition()));
  GoToStep(kMatchSuffix);
}

void TextFragmentFinder::FindSuffix() {
  EphemeralRangeInFlatTree potential_match(potential_match_);
  DCHECK(!potential_match.IsNull());

  if (selector_.Suffix().IsEmpty()) {
    OnMatchComplete();
    return;
  }

  // Now we just have to ensure the match is followed by the |suffix|.
  search_range_->setStart(ToPositionInDOMTree(
      NextTextPosition(potential_match.EndPosition(),
                       ToPositionInFlatTree(match_range_->EndPosition()))));
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
  EphemeralRangeInFlatTree potential_match(potential_match_);
  EphemeralRangeInFlatTree suffix_range(
      NextTextPosition(potential_match.EndPosition(),
                       ToPositionInFlatTree(match_range_->EndPosition())),
      ToPositionInFlatTree(match_range_->EndPosition()));
  if (suffix_match.StartPosition() == suffix_range.StartPosition()) {
    OnMatchComplete();
    return;
  }

  // If this is an exact match(e.g. |end_text| is not specified), and we
  // didn't match on suffix, continue searching for a new potential_match
  // from it's start.
  if (selector_.End().IsEmpty()) {
    potential_match_.Clear();
    GoToStep(kMatchPrefix);
    return;
  }

  // If this is a range match(e.g. |end_text| is specified), it is possible
  // that we found the correct range start, but not the correct range end.
  // Continue searching for it, without restarting the range start search.
  range_end_search_start_ = potential_match.EndPosition();
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
  if (!start.ComputeContainerNode()->GetLayoutObject() ||
      !end.ComputeContainerNode()->GetLayoutObject())
    return true;
  return FindBuffer::IsInSameUninterruptedBlock(*start.ComputeContainerNode(),
                                                *end.ComputeContainerNode());
}

TextFragmentFinder::TextFragmentFinder(Client& client,
                                       const TextFragmentSelector& selector,
                                       Document* document,
                                       FindBufferRunnerType runner_type)
    : client_(client), selector_(selector), document_(document) {
  DCHECK(!selector_.Start().IsEmpty());
  DCHECK(selector_.Type() != TextFragmentSelector::SelectorType::kInvalid);
  if (runner_type == TextFragmentFinder::FindBufferRunnerType::kAsynchronous) {
    find_buffer_runner_ = MakeGarbageCollected<AsyncFindBuffer>();
  } else {
    find_buffer_runner_ = MakeGarbageCollected<SyncFindBuffer>();
  }
}

void TextFragmentFinder::FindMatch() {
  if (find_buffer_runner_ && find_buffer_runner_->IsActive())
    find_buffer_runner_->Cancel();

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
  search_range_ = Range::Create(*document_);
  search_range_->setStart(ToPositionInDOMTree(search_start));
  search_range_->setEnd(ToPositionInDOMTree(search_end));
  match_range_ = Range::Create(*document_);
  match_range_->setStart(ToPositionInDOMTree(search_start));
  match_range_->setEnd(ToPositionInDOMTree(search_end));
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
    EphemeralRangeInFlatTree match(first_match_);
    FindMatchFromPosition(match.EndPosition());
  } else {
    TextFragmentAnchorMetrics::Match match_metrics(selector_);
    EphemeralRangeInFlatTree potential_match(first_match_);
    if (selector_.Type() == TextFragmentSelector::SelectorType::kExact) {
      // If it's an exact match, we don't need to do the PlainText conversion,
      // we can just use the text from the selector.
      DCHECK_EQ(selector_.Start().length(),
                PlainText(potential_match).length());
      match_metrics.text = selector_.Start();

      if (ContainedByListItem(potential_match)) {
        match_metrics.is_list_item = true;
      }
      if (ContainedByTableCell(potential_match)) {
        match_metrics.is_table_cell = true;
      }
    } else if (selector_.Type() == TextFragmentSelector::SelectorType::kRange) {
      match_metrics.text = PlainText(potential_match);
      match_metrics.spans_multiple_blocks = !IsInSameUninterruptedBlock(
          potential_match.StartPosition(), potential_match.EndPosition());
    }
    client_.DidFindMatch(potential_match, match_metrics, !potential_match_);
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
  potential_match_ = MakeGarbageCollected<Range>(
      range.GetDocument(), ToPositionInDOMTree(range.StartPosition()),
      ToPositionInDOMTree(range.EndPosition()));
}

void TextFragmentFinder::SetPrefixMatch(EphemeralRangeInFlatTree range) {
  prefix_match_ = MakeGarbageCollected<Range>(
      range.GetDocument(), ToPositionInDOMTree(range.StartPosition()),
      ToPositionInDOMTree(range.EndPosition()));
}
}  // namespace blink
