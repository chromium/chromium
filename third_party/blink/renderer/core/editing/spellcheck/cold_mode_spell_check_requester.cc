// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/cold_mode_spell_check_requester.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/idle_deadline.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

const int kColdModeChunkSize = 16384;  // in UTF16 code units
const int kInvalidChunkIndex = -1;

}  // namespace

void ColdModeSpellCheckRequester::Trace(Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(root_editable_);
  visitor->Trace(remaining_check_range_);
}

ColdModeSpellCheckRequester::ColdModeSpellCheckRequester(LocalFrame& frame)
    : frame_(frame),
      last_chunk_index_(kInvalidChunkIndex),
      needs_more_invocation_for_testing_(false) {}

bool ColdModeSpellCheckRequester::FullyChecked() const {
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
  return GetFrame().GetSpellChecker().GetSpellCheckRequester();
}

const Element* ColdModeSpellCheckRequester::CurrentFocusedEditable() const {
  const Position position =
      GetFrame().Selection().GetSelectionInDOMTree().Extent();
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
  GetFrame().GetDocument()->UpdateStyleAndLayout();

  const Element* current_focused = CurrentFocusedEditable();
  if (!current_focused) {
    ClearProgress();
    return;
  }

  if (root_editable_ != current_focused) {
    ClearProgress();
    root_editable_ = current_focused;
    last_chunk_index_ = 0;
    remaining_check_range_ = Range::Create(root_editable_->GetDocument());
    remaining_check_range_->selectNodeContents(
        const_cast<Element*>(root_editable_.Get()), ASSERT_NO_EXCEPTION);
  }

  while (deadline->timeRemaining() > 0) {
    if (FullyChecked()) {
      SetHasFullyChecked();
      return;
    }
    RequestCheckingForNextChunk();
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

void ColdModeSpellCheckRequester::SetHasFullyChecked() {
  DCHECK(root_editable_);
  last_chunk_index_ = kInvalidChunkIndex;
  if (!remaining_check_range_)
    return;
  remaining_check_range_->Dispose();
  remaining_check_range_ = nullptr;
}

void ColdModeSpellCheckRequester::RequestCheckingForNextChunk() {
  DCHECK(root_editable_);
  DCHECK(!FullyChecked());

  const EphemeralRange remaining_range(remaining_check_range_);
  const int remaining_length = TextIterator::RangeLength(
      remaining_range,
      // Same behavior used in |CalculateCharacterSubrange()|
      TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
  if (remaining_length == 0) {
    SetHasFullyChecked();
    return;
  }

  const int chunk_index = last_chunk_index_ + 1;
  const Position chunk_start = remaining_range.StartPosition();
  const Position chunk_end =
      CalculateCharacterSubrange(remaining_range, 0,
                                 std::min(remaining_length, kColdModeChunkSize))
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
}

}  // namespace blink
