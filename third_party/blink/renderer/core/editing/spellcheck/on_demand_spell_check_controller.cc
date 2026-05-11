// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/on_demand_spell_check_controller.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

inline constexpr int kOnDemandFullCheckingChunkSize = 1000;

}  // namespace

OnDemandSpellCheckController::OnDemandSpellCheckController(
    LocalDOMWindow& window,
    SpellCheckRequester& requester)
    : ExecutionContextLifecycleObserver(&window),
      window_(window),
      spell_check_requester_(requester) {}

OnDemandSpellCheckController::~OnDemandSpellCheckController() = default;

OnDemandSpellCheckController::State OnDemandSpellCheckController::GetState()
    const {
  return state_;
}

void OnDemandSpellCheckController::SetSpellCheckingDisabled(
    const Element& element) {
  if (root_editable_ == &element) {
    ClearProgress();
  }
}

void OnDemandSpellCheckController::ElementRemoved(const Element& element) {
  if (root_editable_ == &element) {
    ClearProgress();
  }
}

void OnDemandSpellCheckController::RequestFullChecking(
    const ContainerNode* container_node) {
  if (!IsSpellCheckingEnabled() ||
      !SpellChecker::IsSpellCheckingEnabledAt(Position(container_node, 0))) {
    ClearProgress();
    return;
  }

  const Element* focused_element = window_->document()->FocusedElement();
  const bool has_transient_user_activation =
      LocalFrame::HasTransientUserActivation(window_->GetFrame());
  const bool is_focused_element_in_container =
      focused_element && container_node->contains(focused_element);
  if (!has_transient_user_activation &&
      (!is_focused_element_in_container ||
       !focused_element->WasLastFocusFromUserGesture()) &&
      !base::FeatureList::IsEnabled(
          features::kUnrestrictSpellingAndGrammarForTesting)) {
    ClearProgress();
    return;
  }

  // Some IMEs' functionalities (e.g. Gboard's Add to Personal Dictionary) rely
  // on PerformFullContentSpellCheck to perform a full-content spell check. In
  // such case, the spell check request cache could have become stale by the
  // time this method is called. Therefore, we want to force a fresh request to
  // spell check service.
  if (!RuntimeEnabledFeatures::SpellCheckChunkingEnabled()) {
    const EphemeralRange range(Position(container_node, 0),
                               Position::LastPositionInNode(*container_node));
    spell_check_requester_->RequestCheckingFor(range, /*request_num=*/0,
                                               /*should_force_refresh=*/true);
    return;
  }

  if (root_editable_ != container_node) {
    ClearProgress();
    root_editable_ = container_node;
    last_chunk_index_ = 0;
    remaining_check_range_ = Range::Create(root_editable_->GetDocument());
    remaining_check_range_->selectNodeContents(
        const_cast<ContainerNode*>(root_editable_.Get()), ASSERT_NO_EXCEPTION);
  }

  if (FullyCheckedCurrentRootEditable() || GetState() == State::kInProgress) {
    return;
  }

  task_handle_ = PostCancellableTask(
      *window_->GetTaskRunner(TaskType::kInternalUserInteraction), FROM_HERE,
      blink::BindOnce(
          &OnDemandSpellCheckController::RequestCheckingForRemainingRange,
          WrapPersistent(weak_factory_.GetWeakCell())));
}

void OnDemandSpellCheckController::ClearProgress() {
  state_ = State::kInactive;
  task_handle_.Cancel();
  root_editable_ = nullptr;

  if (remaining_check_range_) {
    remaining_check_range_->Dispose();
    remaining_check_range_ = nullptr;
  }
  last_chunk_index_ = -1;
  weak_factory_.Invalidate();
}

void OnDemandSpellCheckController::ContextDestroyed() {
  ClearProgress();
}

bool OnDemandSpellCheckController::IsSpellCheckingEnabled() const {
  if (!GetExecutionContext()) {
    return false;
  }
  return window_->GetSpellChecker().IsSpellCheckingEnabled();
}

void OnDemandSpellCheckController::RequestCheckingForRemainingRange() {
  // SpellCheckRequester will handle the sequential delivery to the spell check
  // service.
  if (FullyCheckedCurrentRootEditable()) {
    ClearProgress();
    return;
  }

  const EphemeralRange remaining_range(remaining_check_range_);
  CharacterIterator it(
      remaining_range,
      TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
  if (it.AtEnd()) {
    ClearProgress();
    return;
  }

  state_ = State::kInProgress;

  // Each chunk starts at the start of the remaining range.
  const Position chunk_start = remaining_range.StartPosition();

  // The end of the current chunk, limited by the chunk size.
  const Position chunk_end =
      it.CalculateCharacterSubrange(0, kOnDemandFullCheckingChunkSize)
          .EndPosition();

  // The end of the sentence containing the chunk end.
  const Position extended_end = EndOfSentence(chunk_end).GetPosition();

  // The final end of the current check range, which is the sentence end if
  // valid, but capped by the remaining range.
  const Position check_end =
      extended_end.IsNull() || extended_end < chunk_end
          ? chunk_end
          : std::min(extended_end, remaining_range.EndPosition());
  const EphemeralRange check_range(chunk_start, check_end);

  if (!spell_check_requester_) {
    ClearProgress();
    return;
  }

  last_chunk_index_++;
  DCHECK_GE(last_chunk_index_, 0);
  spell_check_requester_->RequestCheckingFor(check_range,
                                             /*request_num=*/last_chunk_index_,
                                             /*should_force_refresh=*/true);

  if (!remaining_check_range_) {
    ClearProgress();
    return;
  }

  remaining_check_range_->setStart(check_range.EndPosition());

  task_handle_ = PostCancellableTask(
      *window_->GetTaskRunner(TaskType::kInternalUserInteraction), FROM_HERE,
      blink::BindOnce(
          &OnDemandSpellCheckController::RequestCheckingForRemainingRange,
          WrapPersistent(weak_factory_.GetWeakCell())));
}

bool OnDemandSpellCheckController::FullyCheckedCurrentRootEditable() const {
  return !root_editable_ || !remaining_check_range_ ||
         remaining_check_range_->collapsed() ||
         !remaining_check_range_->IsConnected() ||
         !root_editable_->contains(
             remaining_check_range_->commonAncestorContainer());
}

void OnDemandSpellCheckController::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  visitor->Trace(spell_check_requester_);
  visitor->Trace(root_editable_);
  visitor->Trace(remaining_check_range_);
  visitor->Trace(weak_factory_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
