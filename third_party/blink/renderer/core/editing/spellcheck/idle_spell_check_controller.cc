// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"

#include "base/check_deref.h"
#include "base/debug/crash_logging.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/editing/commands/undo_stack.h"
#include "third_party/blink/renderer/core/editing/commands/undo_step.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/cold_mode_spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/hot_mode_spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

namespace {

constexpr base::TimeDelta kColdModeTimerInterval = base::Milliseconds(1000);
constexpr base::TimeDelta kConsecutiveColdModeTimerInterval =
    base::Milliseconds(200);
const int kHotModeRequestTimeoutMS = 200;
const int kInvalidHandle = -1;
const int kDummyHandleForForcedInvocation = -2;
constexpr base::TimeDelta kIdleSpellcheckTestTimeout = base::Seconds(10);

}  // namespace

class IdleSpellCheckController::IdleCallback final : public IdleTask {
 public:
  explicit IdleCallback(IdleSpellCheckController* controller)
      : controller_(controller) {}
  IdleCallback(const IdleCallback&) = delete;
  IdleCallback& operator=(const IdleCallback&) = delete;

  void Trace(Visitor* visitor) const final {
    visitor->Trace(controller_);
    IdleTask::Trace(visitor);
  }

 private:
  void invoke(IdleDeadline* deadline) final { controller_->Invoke(deadline); }

  const Member<IdleSpellCheckController> controller_;
};

IdleSpellCheckController::~IdleSpellCheckController() = default;

void IdleSpellCheckController::Trace(Visitor* visitor) const {
  visitor->Trace(cold_mode_requester_);
  visitor->Trace(spell_check_requeseter_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

IdleSpellCheckController::IdleSpellCheckController(
    LocalDOMWindow& window,
    SpellCheckRequester& requester)
    : ExecutionContextLifecycleObserver(&window),
      idle_callback_handle_(kInvalidHandle),
      cold_mode_requester_(
          MakeGarbageCollected<ColdModeSpellCheckRequester>(window)),
      spell_check_requeseter_(requester) {}

LocalDOMWindow& IdleSpellCheckController::GetWindow() const {
  DCHECK(GetExecutionContext());
  return *To<LocalDOMWindow>(GetExecutionContext());
}

Document& IdleSpellCheckController::GetDocument() const {
  DCHECK(GetExecutionContext());
  return *GetWindow().document();
}

bool IdleSpellCheckController::IsSpellCheckingEnabled() const {
  if (!GetExecutionContext())
    return false;
  return GetWindow().GetSpellChecker().IsSpellCheckingEnabled();
}

void IdleSpellCheckController::DisposeIdleCallback() {
  if (idle_callback_handle_ != kInvalidHandle && GetExecutionContext()) {
    ScriptedIdleTaskController::From(*GetExecutionContext())
        .CancelCallback(idle_callback_handle_);
  }
  idle_callback_handle_ = kInvalidHandle;
}

void IdleSpellCheckController::Deactivate() {
  state_ = State::kInactive;
  if (cold_mode_timer_.IsActive())
    cold_mode_timer_.Cancel();
  cold_mode_requester_->Deactivate();
  DisposeIdleCallback();
  spell_check_requeseter_->Deactivate();
}

void IdleSpellCheckController::RespondToChangedSelection() {
  if (!IsSpellCheckingEnabled()) {
    Deactivate();
    return;
  }

  if (IsInInvocation())
    return;

  needs_invocation_for_changed_selection_ = true;
  SetNeedsInvocation();
}

void IdleSpellCheckController::RespondToChangedContents() {
  if (!IsSpellCheckingEnabled()) {
    Deactivate();
    return;
  }

  if (IsInInvocation())
    return;

  needs_invocation_for_changed_contents_ = true;
  SetNeedsInvocation();
}

void IdleSpellCheckController::RespondToChangedEnablement() {
  if (!IsSpellCheckingEnabled()) {
    Deactivate();
    return;
  }

  if (IsInInvocation())
    return;

  needs_invocation_for_changed_enablement_ = true;
  SetNeedsInvocation();
}

void IdleSpellCheckController::SetNeedsInvocation() {
  DCHECK(IsSpellCheckingEnabled());

  if (state_ == State::kHotModeRequested)
    return;

  cold_mode_requester_->ClearProgress();

  if (state_ == State::kColdModeTimerStarted) {
    DCHECK(cold_mode_timer_.IsActive());
    cold_mode_timer_.Cancel();
  }

  if (state_ == State::kColdModeRequested)
    DisposeIdleCallback();

  IdleRequestOptions* options = IdleRequestOptions::Create();
  options->setTimeout(kHotModeRequestTimeoutMS);
  idle_callback_handle_ =
      ScriptedIdleTaskController::From(CHECK_DEREF(GetExecutionContext()))
          .RegisterCallback(MakeGarbageCollected<IdleCallback>(this), options);
  state_ = State::kHotModeRequested;
}

void IdleSpellCheckController::SetNeedsColdModeInvocation() {
  DCHECK(IsSpellCheckingEnabled());
  if (state_ != State::kInactive && state_ != State::kInHotModeInvocation &&
      state_ != State::kInColdModeInvocation)
    return;

  DCHECK(!cold_mode_timer_.IsActive());
  base::TimeDelta interval = state_ == State::kInColdModeInvocation
                                 ? kConsecutiveColdModeTimerInterval
                                 : kColdModeTimerInterval;
  cold_mode_timer_ = PostDelayedCancellableTask(
      *GetWindow().GetTaskRunner(TaskType::kInternalDefault), FROM_HERE,
      WTF::BindOnce(&IdleSpellCheckController::ColdModeTimerFired,
                    WrapPersistent(this)),
      interval);
  state_ = State::kColdModeTimerStarted;
}

void IdleSpellCheckController::ColdModeTimerFired() {
  DCHECK_EQ(State::kColdModeTimerStarted, state_);

  if (!IsSpellCheckingEnabled()) {
    Deactivate();
    return;
  }

  idle_callback_handle_ =
      ScriptedIdleTaskController::From(CHECK_DEREF(GetExecutionContext()))
          .RegisterCallback(MakeGarbageCollected<IdleCallback>(this),
                            IdleRequestOptions::Create());
  state_ = State::kColdModeRequested;
}

bool IdleSpellCheckController::NeedsHotModeCheckingUnderCurrentSelection()
    const {
  if (needs_invocation_for_changed_contents_ ||
      needs_invocation_for_changed_enablement_) {
    return true;
  }

  // If there's only selection movement, we skip hot mode if cold mode has
  // already fully checked the current element.
  DCHECK(needs_invocation_for_changed_selection_);
  const Position& position =
      GetWindow().GetFrame()->Selection().GetSelectionInDOMTree().Focus();
  const auto* element = DynamicTo<Element>(HighestEditableRoot(position));
  if (!element || !element->isConnected())
    return false;
  return !cold_mode_requester_->HasFullyChecked(*element);
}

void IdleSpellCheckController::HotModeInvocation(IdleDeadline* deadline) {
  TRACE_EVENT0("blink", "IdleSpellCheckController::hotModeInvocation");

  // TODO(xiaochengh): Figure out if this has any performance impact.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  HotModeSpellCheckRequester requester(*spell_check_requeseter_);

  if (NeedsHotModeCheckingUnderCurrentSelection()) {
    requester.CheckSpellingAt(
        GetWindow().GetFrame()->Selection().GetSelectionInDOMTree().Focus());
  }

  const uint64_t watermark = last_processed_undo_step_sequence_;
  for (const UndoStep* step :
       GetWindow().GetFrame()->GetEditor().GetUndoStack().UndoSteps()) {
    if (step->SequenceNumber() <= watermark)
      break;
    last_processed_undo_step_sequence_ =
        std::max(step->SequenceNumber(), last_processed_undo_step_sequence_);
    if (deadline->timeRemaining() == 0)
      break;
    // The ending selection stored in undo stack can be invalid, disconnected
    // or have been moved to another document, so we should check its validity
    // before using it.
    if (!step->EndingSelection().IsValidFor(GetDocument()))
      continue;
    requester.CheckSpellingAt(step->EndingSelection().Focus());
  }

  needs_invocation_for_changed_selection_ = false;
  needs_invocation_for_changed_contents_ = false;
  needs_invocation_for_changed_enablement_ = false;
}

void IdleSpellCheckController::Invoke(IdleDeadline* deadline) {
  DCHECK_NE(idle_callback_handle_, kInvalidHandle);
  idle_callback_handle_ = kInvalidHandle;

  if (!IsSpellCheckingEnabled()) {
    Deactivate();
    return;
  }

  if (state_ == State::kHotModeRequested) {
    state_ = State::kInHotModeInvocation;
    HotModeInvocation(deadline);
    SetNeedsColdModeInvocation();
  } else if (state_ == State::kColdModeRequested) {
    state_ = State::kInColdModeInvocation;
    cold_mode_requester_->Invoke(deadline);
    if (cold_mode_requester_->FullyCheckedCurrentRootEditable()) {
      state_ = State::kInactive;
    } else {
      SetNeedsColdModeInvocation();
    }
  } else {
    // TODO(crbug.com/1424540): The other states are unexpected but reached in
    // real world. We work around it and dump debugging information.
    static auto* state_data = base::debug::AllocateCrashKeyString(
        "spellchecker-state-on-invocation", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(state_data, GetStateAsString());
    DUMP_WILL_BE_NOTREACHED() << GetStateAsString();
    Deactivate();
  }
}

void IdleSpellCheckController::ContextDestroyed() {
  Deactivate();
}

void IdleSpellCheckController::ForceInvocationForTesting() {
  if (!IsSpellCheckingEnabled())
    return;

  bool cross_origin_isolated_capability =
      GetExecutionContext()
          ? GetExecutionContext()->CrossOriginIsolatedCapability()
          : false;

  auto* deadline = MakeGarbageCollected<IdleDeadline>(
      base::TimeTicks::Now() + kIdleSpellcheckTestTimeout,
      cross_origin_isolated_capability,
      IdleDeadline::CallbackType::kCalledWhenIdle);

  switch (state_) {
    case State::kColdModeTimerStarted:
      cold_mode_timer_.Cancel();
      state_ = State::kColdModeRequested;
      idle_callback_handle_ = kDummyHandleForForcedInvocation;
      Invoke(deadline);
      break;
    case State::kHotModeRequested:
    case State::kColdModeRequested:
      if (GetExecutionContext()) {
        ScriptedIdleTaskController::From(*GetExecutionContext())
            .CancelCallback(idle_callback_handle_);
      }
      Invoke(deadline);
      break;
    case State::kInactive:
    case State::kInHotModeInvocation:
    case State::kInColdModeInvocation:
      NOTREACHED_IN_MIGRATION();
  }
}

void IdleSpellCheckController::SkipColdModeTimerForTesting() {
  DCHECK(cold_mode_timer_.IsActive());
  cold_mode_timer_.Cancel();
  ColdModeTimerFired();
}

void IdleSpellCheckController::SetNeedsMoreColdModeInvocationForTesting() {
  cold_mode_requester_->SetNeedsMoreInvocationForTesting();
}

void IdleSpellCheckController::SetSpellCheckingDisabled(
    const Element& element) {
  cold_mode_requester_->RemoveFromFullyChecked(element);
}

const char* IdleSpellCheckController::GetStateAsString() const {
  static const char* const kTexts[] = {
#define V(state) #state,
      FOR_EACH_IDLE_SPELL_CHECK_CONTROLLER_STATE(V)
#undef V
  };

  unsigned index = static_cast<unsigned>(state_);
  if (index < std::size(kTexts)) {
    return kTexts[index];
  }
  return "Invalid";
}

}  // namespace blink
