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
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"

namespace blink {

namespace {

const char kNoContext[] = "";

// Determines whether the start and end positions of |range| are on word
// boundaries.
// TODO(crbug/924965): Determine how this should check node boundaries. This
// treats node boundaries as word boundaries, for example "o" is a whole word
// match in "f<i>o</i>o".
bool IsWholeWordMatch(EphemeralRangeInFlatTree range) {
  wtf_size_t start_position = range.StartPosition().OffsetInContainerNode();

  if (start_position != 0) {
    String start_text = range.StartPosition().AnchorNode()->textContent();
    start_text.Ensure16Bit();
    wtf_size_t word_start = FindWordStartBoundary(
        start_text.Characters16(), start_text.length(), start_position);
    if (word_start != start_position)
      return false;
  }

  wtf_size_t end_position = range.EndPosition().OffsetInContainerNode();
  String end_text = range.EndPosition().AnchorNode()->textContent();

  if (end_position != end_text.length()) {
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

EphemeralRangeInFlatTree FindMatchInRange(String search_text,
                                          PositionInFlatTree search_start,
                                          PositionInFlatTree search_end) {
  while (search_start < search_end) {
    const EphemeralRangeInFlatTree search_range(search_start, search_end);
    EphemeralRangeInFlatTree potential_match = FindBuffer::FindMatchInRange(
        search_range, search_text, kCaseInsensitive);

    if (potential_match.IsNull() || IsWholeWordMatch(potential_match))
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

// Find and return the range of |search_text| if the first text in the search
// range is |search_text|, skipping over whitespace and element boundaries.
EphemeralRangeInFlatTree FindImmediateMatch(String search_text,
                                            PositionInFlatTree search_start,
                                            PositionInFlatTree search_end) {
  if (search_text.IsEmpty())
    return EphemeralRangeInFlatTree();

  search_start = NextTextPosition(search_start, search_end);
  if (search_start == search_end)
    return EphemeralRangeInFlatTree();

  FindBuffer buffer(EphemeralRangeInFlatTree(search_start, search_end));

  // TODO(nburris): FindBuffer will search the rest of the document for a match,
  // but we only need to check for an immediate match, so we should stop
  // searching if there's no immediate match.
  FindBuffer::Results match_results =
      buffer.FindMatches(search_text, kCaseInsensitive);

  if (!match_results.IsEmpty() && match_results.front().start == 0u) {
    FindBuffer::BufferMatchResult buffer_match = match_results.front();
    EphemeralRangeInFlatTree match = buffer.RangeFromBufferIndex(
        buffer_match.start, buffer_match.start + buffer_match.length);
    if (IsWholeWordMatch(match))
      return match;
  }

  return EphemeralRangeInFlatTree();
}

EphemeralRangeInFlatTree FindMatchInRangeWithContext(
    const String& search_text,
    const String& prefix,
    const String& suffix,
    PositionInFlatTree search_start,
    PositionInFlatTree search_end) {
  while (search_start != search_end) {
    EphemeralRangeInFlatTree potential_match;

    if (!prefix.IsEmpty()) {
      EphemeralRangeInFlatTree prefix_match =
          FindMatchInRange(prefix, search_start, search_end);

      // No prefix_match in remaining range
      if (prefix_match.IsNull())
        return EphemeralRangeInFlatTree();

      search_start = prefix_match.EndPosition();
      potential_match =
          FindImmediateMatch(search_text, search_start, search_end);

      // No search_text match after current prefix_match
      if (potential_match.IsNull())
        continue;
    } else {
      potential_match = FindMatchInRange(search_text, search_start, search_end);

      // No search_text match in remaining range
      if (potential_match.IsNull())
        return EphemeralRangeInFlatTree();

      search_start = potential_match.EndPosition();
    }

    PositionInFlatTree suffix_start = potential_match.EndPosition();
    DCHECK(potential_match.IsNotNull());
    if (!suffix.IsEmpty()) {
      EphemeralRangeInFlatTree suffix_match =
          FindImmediateMatch(suffix, suffix_start, search_end);

      // No suffix match after current potential_match
      if (suffix_match.IsNull())
        continue;
    }

    // If we reach here without a return or continue, we have a full match.
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

  // TODO(crbug.com/930156): Make FindMatch work asynchronously.
  EphemeralRangeInFlatTree match;
  if (selector_.Type() == TextFragmentSelector::kExact) {
    match = FindMatchInRangeWithContext(selector_.Start(), selector_.Prefix(),
                                        selector_.Suffix(), search_start,
                                        search_end);
  } else {
    EphemeralRangeInFlatTree start_match =
        FindMatchInRangeWithContext(selector_.Start(), selector_.Prefix(),
                                    kNoContext, search_start, search_end);
    if (start_match.IsNull())
      return start_match;

    // TODO(crbug.com/924964): Determine what we should do if the start text and
    // end text are the same (and there are no context terms). This
    // implementation continues searching for the next instance of the text,
    // from the end of the first instance.
    search_start = start_match.EndPosition();
    EphemeralRangeInFlatTree end_match = FindMatchInRangeWithContext(
        selector_.End(), kNoContext, selector_.Suffix(), search_start,
        search_end);
    if (end_match.IsNotNull()) {
      match = EphemeralRangeInFlatTree(start_match.StartPosition(),
                                       end_match.EndPosition());
    }
  }

  return match;
}

}  // namespace blink
