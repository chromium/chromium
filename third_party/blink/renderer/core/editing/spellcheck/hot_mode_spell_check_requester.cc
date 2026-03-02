// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/hot_mode_spell_check_requester.h"

#include "third_party/blink/renderer/core/editing/commands/composite_edit_command.h"
#include "third_party/blink/renderer/core/editing/commands/typing_command.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/backwards_character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

namespace {

const int kHotModeCheckAllThreshold = 128;
const int kHotModeChunkSize = 1024;

EphemeralRange AdjacentWordIfExists(const Position& pos) {
  const Position word_start = PreviousWordPosition(pos).GetPosition();
  if (word_start.IsNull())
    return EphemeralRange();
  const Position word_end = EndOfWordPosition(word_start);
  if (word_end.IsNull())
    return EphemeralRange();
  if (ComparePositions(pos, word_end) > 0)
    return EphemeralRange();
  return EphemeralRange(word_start, word_end);
}

EphemeralRange CurrentWordIfTypingInPartialWord(const Element& editable) {
  const LocalFrame& frame = *editable.GetDocument().GetFrame();
  const SelectionInDOMTree& selection =
      frame.Selection().GetSelectionInDOMTree();
  if (!selection.IsCaret())
    return EphemeralRange();
  if (RootEditableElementOf(selection.Anchor()) != &editable) {
    return EphemeralRange();
  }

  CompositeEditCommand* last_command = frame.GetEditor().LastEditCommand();
  if (!last_command || !last_command->IsTypingCommand())
    return EphemeralRange();
  if (!last_command->EndingSelection().IsValidFor(*frame.GetDocument()))
    return EphemeralRange();
  if (last_command->EndingSelection().AsSelection() != selection)
    return EphemeralRange();
  return AdjacentWordIfExists(selection.Anchor());
}

EphemeralRange CalculateHotModeCheckingRange(const Element& editable,
                                             const Position& position) {
  // Check everything in |editable| if its total length is short.
  const EphemeralRange& full_range = EphemeralRange::RangeOfContents(editable);
  const int full_length = TextIterator::RangeLength(full_range);
  // TODO(xiaochengh): There is no need to check if |full_length <= 2|, since
  // we don't consider two characters as misspelled. However, a lot of layout
  // tests depend on "zz" as misspelled, which should be changed.
  if (full_length <= kHotModeCheckAllThreshold)
    return full_range;

  // Otherwise, if |position| is in a short paragraph, check the paragraph.
  const EphemeralRange& paragraph_range =
      ExpandToParagraphBoundary(EphemeralRange(position));
  const int paragraph_length = TextIterator::RangeLength(paragraph_range);
  if (paragraph_length <= kHotModeChunkSize)
    return paragraph_range;

  // Otherwise, check a chunk of text centered at |position|.
  TextIteratorBehavior behavior =
      TextIteratorBehavior::Builder()
          .SetEmitsObjectReplacementCharacter(true)
          .SetEmitsPunctuationForReplacedElements(true)
          .Build();
  BackwardsCharacterIterator backward_iterator(
      EphemeralRange(full_range.StartPosition(), position), behavior);
  if (!backward_iterator.AtEnd())
    backward_iterator.Advance(kHotModeChunkSize / 2);
  const Position& chunk_start = backward_iterator.EndPosition();
  CharacterIterator forward_iterator(position, full_range.EndPosition(),
                                     behavior);
  if (!forward_iterator.AtEnd())
    forward_iterator.Advance(kHotModeChunkSize / 2);
  const Position& chunk_end = forward_iterator.EndPosition();
  return ExpandRangeToSentenceBoundary(EphemeralRange(chunk_start, chunk_end));
}

}  // namespace

HotModeSpellCheckRequester::HotModeSpellCheckRequester(
    SpellCheckRequester& requester)
    : requester_(&requester) {}

void HotModeSpellCheckRequester::CheckSpellingAt(const Position& position) {
  const Element* root_editable = RootEditableElementOf(position);
  if (!root_editable || !root_editable->isConnected())
    return;

  if (processed_root_editables_.Contains(root_editable))
    return;
  processed_root_editables_.push_back(root_editable);

  if (!root_editable->IsSpellCheckingEnabled() &&
      !SpellChecker::IsSpellCheckingEnabledAt(position)) {
    return;
  }

  const EphemeralRange& current_word =
      CurrentWordIfTypingInPartialWord(*root_editable);
  if (current_word.IsNotNull()) {
    root_editable->GetDocument().Markers().RemoveMarkersInRange(
        current_word, DocumentMarker::MarkerTypes::Misspelling());
    return;
  }

  const EphemeralRange& checking_range =
      CalculateHotModeCheckingRange(*root_editable, position);
  requester_->RequestCheckingFor(checking_range);
}

}  // namespace blink
