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
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_iterator.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"

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

EphemeralRangeInFlatTree FindMatchInRange(String search_text,
                                          PositionInFlatTree search_start,
                                          PositionInFlatTree search_end,
                                          bool word_start_bounded,
                                          bool word_end_bounded) {
  while (search_start < search_end) {
    const EphemeralRangeInFlatTree search_range(search_start, search_end);
    EphemeralRangeInFlatTree potential_match = FindBuffer::FindMatchInRange(
        search_range, search_text, kCaseInsensitive);

    if (potential_match.IsNull() ||
        IsWordBounded(potential_match, word_start_bounded, word_end_bounded))
      return potential_match;

    search_start = potential_match.EndPosition();
  }

  return EphemeralRangeInFlatTree();
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

EphemeralRangeInFlatTree FindMatchInRangeWithContext(
    const String& start_text,
    const String& end_text,
    const String& prefix,
    const String& suffix,
    PositionInFlatTree search_start,
    PositionInFlatTree search_end) {
  while (search_start != search_end) {
    EphemeralRangeInFlatTree potential_match;

    if (!prefix.IsEmpty()) {
      EphemeralRangeInFlatTree prefix_match = FindMatchInRange(
          prefix, search_start, search_end, /*word_start_bounded=*/true,
          /*word_end_bounded=*/false);

      // No prefix_match in remaining range
      if (prefix_match.IsNull())
        return EphemeralRangeInFlatTree();

      // If we iterate again, start searching from the first boundary after the
      // prefix start (since prefix must start at a boundary). Note, we don't
      // advance to the prefix end; this is done since, if this prefix isn't
      // the one we're looking for, the next occurrence might be overlapping
      // with the current one. e.g. If |prefix| is "a a" and our search range
      // currently starts with "a a a b...", the next iteration should start at
      // the second a which is part of the current |prefix_match|.
      search_start = FirstWordBoundaryAfter(prefix_match.StartPosition());

      EphemeralRangeInFlatTree match_range(
          NextTextPosition(prefix_match.EndPosition(), search_end), search_end);

      // The match text need not be bounded at the end. If this is an exact
      // match (i.e. no |end_text|) and we have a suffix then the suffix will
      // be required to end on the word boundary instead. Since we have a
      // prefix, we don't need the match to be word bounded. See
      // https://github.com/WICG/scroll-to-text-fragment/issues/137 for
      // details.
      const bool end_at_word_boundary = !end_text.IsEmpty() || suffix.IsEmpty();

      potential_match = FindMatchInRange(
          start_text, match_range.StartPosition(), match_range.EndPosition(),
          /*word_start_bounded=*/false, end_at_word_boundary);

      // No start_text match after current prefix_match
      if (potential_match.IsNull())
        return EphemeralRangeInFlatTree();

      // We found a potential match but it didn't immediately follow the prefix.
      if (potential_match.StartPosition() != match_range.StartPosition())
        continue;
    } else {
      const bool end_at_word_boundary = !end_text.IsEmpty() || suffix.IsEmpty();

      potential_match =
          FindMatchInRange(start_text, search_start, search_end,
                           /*word_start_bounded=*/true, end_at_word_boundary);

      // No start_text match in remaining range
      if (potential_match.IsNull())
        return EphemeralRangeInFlatTree();

      search_start = FirstWordBoundaryAfter(potential_match.StartPosition());
    }

    // If we've gotten here, we've found a |prefix| (if one was specified)
    // that's followed by the |start_text|. We'll now try to expand that into a
    // range match if |end_text| is specified.
    if (!end_text.IsEmpty()) {
      EphemeralRangeInFlatTree text_end_range(potential_match.EndPosition(),
                                              search_end);
      const bool end_at_word_boundary = suffix.IsEmpty();

      EphemeralRangeInFlatTree text_end_match =
          FindMatchInRange(end_text, text_end_range.StartPosition(),
                           text_end_range.EndPosition(),
                           /*word_start_bounded=*/true, end_at_word_boundary);

      if (text_end_match.IsNull())
        return EphemeralRangeInFlatTree();

      potential_match = EphemeralRangeInFlatTree(
          potential_match.StartPosition(), text_end_match.EndPosition());
    }

    DCHECK(!potential_match.IsNull());
    if (suffix.IsEmpty())
      return potential_match;

    // Now we just have to ensure the match is followed by the |suffix|.
    EphemeralRangeInFlatTree suffix_range(
        NextTextPosition(potential_match.EndPosition(), search_end),
        search_end);

    EphemeralRangeInFlatTree suffix_match = FindMatchInRange(
        suffix, suffix_range.StartPosition(), suffix_range.EndPosition(),
        /*word_start_bounded=*/false, /*word_end_bounded=*/true);

    // If no suffix appears in what follows the match, there's no way we can
    // possibly satisfy the constraints so bail.
    if (suffix_match.IsNull())
      return EphemeralRangeInFlatTree();

    if (suffix_match.StartPosition() == suffix_range.StartPosition())
      return potential_match;
  }

  return EphemeralRangeInFlatTree();
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

// static
bool TextFragmentFinder::IsInSameUninterruptedBlock(
    const PositionInFlatTree& start,
    const PositionInFlatTree& end) {
  Node* start_node = start.ComputeContainerNode();
  Node* end_node = end.ComputeContainerNode();

  if (start_node->isSameNode(end_node))
    return true;

  Node& start_ancestor =
      FindBuffer::GetFirstBlockLevelAncestorInclusive(*start_node);
  Node& end_ancestor =
      FindBuffer::GetFirstBlockLevelAncestorInclusive(*end_node);

  if (!start_ancestor.isSameNode(&end_ancestor))
    return false;

  Node* node = start_node;
  while (!node->isSameNode(end_node)) {
    if (FindBuffer::IsNodeBlockLevel(*node))
      return false;
    node = FlatTreeTraversal::Next(*node);
  }
  return true;
}

TextFragmentFinder::TextFragmentFinder(Client& client,
                                       const TextFragmentSelector& selector)
    : client_(client), selector_(selector) {
  DCHECK(!selector_.Start().IsEmpty());
  DCHECK(selector_.Type() != TextFragmentSelector::SelectorType::kInvalid);
}

void TextFragmentFinder::FindMatch(Document& document) {
  PositionInFlatTree search_start =
      PositionInFlatTree::FirstPositionInNode(document);

  auto forced_lock_scope =
      document.GetDisplayLockDocumentState().GetScopedForceActivatableLocks();
  document.UpdateStyleAndLayout(DocumentUpdateReason::kFindInPage);

  EphemeralRangeInFlatTree match =
      FindMatchFromPosition(document, search_start);

  if (match.IsNotNull()) {
    TextFragmentAnchorMetrics::Match match_metrics(selector_);

    if (selector_.Type() == TextFragmentSelector::SelectorType::kExact) {
      // If it's an exact match, we don't need to do the PlainText conversion,
      // we can just use the text from the selector.
      DCHECK_EQ(selector_.Start().length(), PlainText(match).length());
      match_metrics.text = selector_.Start();

      if (ContainedByListItem(match)) {
        match_metrics.is_list_item = true;
      }
      if (ContainedByTableCell(match)) {
        match_metrics.is_table_cell = true;
      }
    } else if (selector_.Type() == TextFragmentSelector::SelectorType::kRange) {
      match_metrics.text = PlainText(match);
      match_metrics.spans_multiple_blocks = !IsInSameUninterruptedBlock(
          match.StartPosition(), match.EndPosition());
    }

    // Continue searching to see if we have an ambiguous selector.
    // TODO(crbug.com/919204): This is temporary and only for measuring
    // ambiguous matching during prototyping.
    EphemeralRangeInFlatTree ambiguous_match =
        FindMatchFromPosition(document, match.EndPosition());
    client_.DidFindMatch(match, match_metrics, ambiguous_match.IsNull());
  } else {
    client_.NoMatchFound();
  }
}

EphemeralRangeInFlatTree TextFragmentFinder::FindMatchFromPosition(
    Document& document,
    PositionInFlatTree search_start) {
  PositionInFlatTree search_end;
  if (document.documentElement() && document.documentElement()->lastChild()) {
    search_end =
        PositionInFlatTree::AfterNode(*document.documentElement()->lastChild());
  } else {
    search_end = PositionInFlatTree::LastPositionInNode(document);
  }

  return FindMatchInRangeWithContext(selector_.Start(), selector_.End(),
                                     selector_.Prefix(), selector_.Suffix(),
                                     search_start, search_end);
}

}  // namespace blink
