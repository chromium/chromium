// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/cold_mode_spell_check_requester.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/backwards_character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/scheduler/idle_deadline.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// in UTF16 code units
const int kColdModeFullCheckingChunkSize = 16384;
const int kColdModeLocalCheckingSize = 128;
const int kRecheckThreshold = 1024;

const int kInvalidChunkIndex = -1;

int TotalTextLength(const Element& root_editable) {
  const EphemeralRange& full_range =
      EphemeralRange::RangeOfContents(root_editable);
  return TextIterator::RangeLength(full_range);
}

}  // namespace

void ColdModeSpellCheckRequester::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  visitor->Trace(root_editable_);
  visitor->Trace(remaining_check_range_);
  visitor->Trace(fully_checked_root_editables_);
}

ColdModeSpellCheckRequester::ColdModeSpellCheckRequester(LocalDOMWindow& window)
    : window_(window),
      last_chunk_index_(kInvalidChunkIndex),
      needs_more_invocation_for_testing_(false) {}

bool ColdModeSpellCheckRequester::FullyCheckedCurrentRootEditable() const {
  if (needs_more_invocation_for_testing_) {
    needs_more_invocation_for_testing_ = false;
    return false;
  }
  // Note that DOM mutations between cold mode invocations may corrupt the
  // stored states, in which case we also consider checking as finished.
  return !root_editable_ || !remaining_check_range_ ||
         remaining_check_range_->collapsed() ||
         !remaining_check_range_->IsConnected() ||
         !root_editable_->contains(
             remaining_check_range_->commonAncestorContainer());
}

SpellCheckRequester& ColdModeSpellCheckRequester::GetSpellCheckRequester()
    const {
  return window_->GetSpellChecker().GetSpellCheckRequester();
}

const Element* ColdModeSpellCheckRequester::CurrentFocusedEditable() const {
  const Position position =
      window_->GetFrame()->Selection().GetSelectionInDOMTree().Focus();
  if (position.IsNull())
    return nullptr;

  const auto* element = DynamicTo<Element>(HighestEditableRoot(position));
  if (!element || !element->isConnected())
    return nullptr;

  if (!element->IsSpellCheckingEnabled() ||
      !SpellChecker::IsSpellCheckingEnabledAt(position))
    return nullptr;

  return element;
}

void ColdModeSpellCheckRequester::Invoke(IdleDeadline* deadline) {
  TRACE_EVENT0("blink", "ColdModeSpellCheckRequester::invoke");

  // TODO(xiaochengh): Figure out if this has any performance impact.
  window_->document()->UpdateStyleAndLayout(DocumentUpdateReason::kSpellCheck);

  const Element* current_focused = CurrentFocusedEditable();
  if (!current_focused) {
    ClearProgress();
    return;
  }

  switch (AccumulateTextDeltaAndComputeCheckingType(*current_focused)) {
    case CheckingType::kNone:
      return;
    case CheckingType::kLocal:
      return RequestLocalChecking(*current_focused);
    case CheckingType::kFull:
      return RequestFullChecking(*current_focused, deadline);
  }
}

void ColdModeSpellCheckRequester::RequestFullChecking(
    const Element& element_to_check,
    IdleDeadline* deadline) {
  TRACE_EVENT0("blink", "ColdModeSpellCheckRequester::RequestFullChecking");

  if (root_editable_ != &element_to_check) {
    ClearProgress();
    root_editable_ = &element_to_check;
    last_chunk_index_ = 0;
    remaining_check_range_ = Range::Create(root_editable_->GetDocument());
    remaining_check_range_->selectNodeContents(
        const_cast<Element*>(root_editable_.Get()), ASSERT_NO_EXCEPTION);
  }

  while (deadline->timeRemaining() > 0) {
    if (FullyCheckedCurrentRootEditable() || !RequestCheckingForNextChunk()) {
      SetHasFullyCheckedCurrentRootEditable();
      return;
    }
  }
}

void ColdModeSpellCheckRequester::ClearProgress() {
  root_editable_ = nullptr;
  last_chunk_index_ = kInvalidChunkIndex;
  if (!remaining_check_range_)
    return;
  remaining_check_range_->Dispose();
  remaining_check_range_ = nullptr;
}

void ColdModeSpellCheckRequester::Deactivate() {
  ClearProgress();
  fully_checked_root_editables_.clear();
}

void ColdModeSpellCheckRequester::SetHasFullyCheckedCurrentRootEditable() {
  DCHECK(root_editable_);
  DCHECK(!fully_checked_root_editables_.Contains(root_editable_));

  fully_checked_root_editables_.Set(
      root_editable_, FullyCheckedEditableEntry{
                          TotalTextLength(*root_editable_), 0,
                          root_editable_->GetDocument().DomTreeVersion()});
  last_chunk_index_ = kInvalidChunkIndex;
  if (!remaining_check_range_)
    return;
  remaining_check_range_->Dispose();
  remaining_check_range_ = nullptr;
}

bool ColdModeSpellCheckRequester::RequestCheckingForNextChunk() {
  DCHECK(root_editable_);
  DCHECK(!FullyCheckedCurrentRootEditable());

  const EphemeralRange remaining_range(remaining_check_range_);
  const int remaining_length = TextIterator::RangeLength(
      remaining_range,
      // Same behavior used in |CalculateCharacterSubrange()|
      TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
  if (remaining_length == 0)
    return false;

  const int chunk_index = last_chunk_index_ + 1;
  const Position chunk_start = remaining_range.StartPosition();
  const Position chunk_end =
      CalculateCharacterSubrange(
          remaining_range, 0,
          std::min(remaining_length, kColdModeFullCheckingChunkSize))
          .EndPosition();

  // Chromium spellchecker requires complete sentences to be checked. However,
  // EndOfSentence() sometimes returns null or out-of-editable positions, which
  // are corrected here.
  const Position extended_end = EndOfSentence(chunk_end).GetPosition();
  const Position check_end =
      extended_end.IsNull() || extended_end < chunk_end
          ? chunk_end
          : std::min(extended_end, remaining_range.EndPosition());
  const EphemeralRange check_range(chunk_start, check_end);

  GetSpellCheckRequester().RequestCheckingFor(check_range, chunk_index);

  last_chunk_index_ = chunk_index;
  remaining_check_range_->setStart(check_range.EndPosition());
  return true;
}

ColdModeSpellCheckRequester::CheckingType
ColdModeSpellCheckRequester::AccumulateTextDeltaAndComputeCheckingType(
    const Element& element_to_check) {
  // Do full checking if we haven't done that before
  auto iter = fully_checked_root_editables_.find(&element_to_check);
  if (iter == fully_checked_root_editables_.end())
    return CheckingType::kFull;

  uint64_t dom_tree_version = element_to_check.GetDocument().DomTreeVersion();

  // Nothing to check, because nothing has changed.
  if (dom_tree_version == iter->value.previous_checked_dom_tree_version) {
    return CheckingType::kNone;
  }
  iter->value.previous_checked_dom_tree_version =
      element_to_check.GetDocument().DomTreeVersion();

  // Compute the amount of text change heuristically. Invoke a full check if
  // the accumulated change is above a threshold; or a local check otherwise.

  int current_text_length = TotalTextLength(element_to_check);
  int delta =
      std::abs(current_text_length - iter->value.previous_checked_length);

  iter->value.accumulated_delta += delta;
  iter->value.previous_checked_length = current_text_length;

  if (iter->value.accumulated_delta > kRecheckThreshold) {
    fully_checked_root_editables_.erase(iter);
    return CheckingType::kFull;
  }

  return CheckingType::kLocal;
}

void ColdModeSpellCheckRequester::RequestLocalChecking(
    const Element& element_to_check) {
  TRACE_EVENT0("blink", "ColdModeSpellCheckRequester::RequestLocalChecking");

  const EphemeralRange& full_range =
      EphemeralRange::RangeOfContents(element_to_check);
  const Position position =
      window_->GetFrame()->Selection().GetSelectionInDOMTree().Focus();
  DCHECK(position.IsNotNull());

  TextIteratorBehavior behavior =
      TextIteratorBehavior::Builder()
          .SetEmitsObjectReplacementCharacter(true)
          .SetEmitsPunctuationForReplacedElements(true)
          .Build();
  BackwardsCharacterIterator backward_iterator(
      EphemeralRange(full_range.StartPosition(), position), behavior);
  if (!backward_iterator.AtEnd())
    backward_iterator.Advance(kColdModeLocalCheckingSize / 2);
  const Position& chunk_start = backward_iterator.EndPosition();
  CharacterIterator forward_iterator(position, full_range.EndPosition(),
                                     behavior);
  if (!forward_iterator.AtEnd())
    forward_iterator.Advance(kColdModeLocalCheckingSize / 2);
  const Position& chunk_end = forward_iterator.EndPosition();
  EphemeralRange checking_range =
      ExpandRangeToSentenceBoundary(EphemeralRange(chunk_start, chunk_end));

  GetSpellCheckRequester().RequestCheckingFor(checking_range);
}

void ColdModeSpellCheckRequester::ElementRemoved(Element* element) {
  if (root_editable_ == element) {
    ClearProgress();
  }
}

}  // namespace blink
