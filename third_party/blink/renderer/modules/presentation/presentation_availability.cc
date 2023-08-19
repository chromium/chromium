// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_availability.h"

#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// static
PresentationAvailability* PresentationAvailability::Take(
    PresentationAvailabilityProperty* resolver,
    const WTF::Vector<KURL>& urls,
    bool value) {
  PresentationAvailability* presentation_availability =
      MakeGarbageCollected<PresentationAvailability>(
          resolver->GetExecutionContext(), urls, value);
  presentation_availability->UpdateStateIfNeeded();
  presentation_availability->UpdateListening();
  return presentation_availability;
}

PresentationAvailability::PresentationAvailability(
    ExecutionContext* execution_context,
    const WTF::Vector<KURL>& urls,
    bool value)
    : ActiveScriptWrappable<PresentationAvailability>({}),
      ExecutionContextLifecycleStateObserver(execution_context),
      PageVisibilityObserver(
          To<LocalDOMWindow>(execution_context)->GetFrame()->GetPage()),
      urls_(urls),
      value_(value),
      state_(State::kActive) {}

PresentationAvailability::~PresentationAvailability() = default;

const AtomicString& PresentationAvailability::InterfaceName() const {
  return event_target_names::kPresentationAvailability;
}

ExecutionContext* PresentationAvailability::GetExecutionContext() const {
  return ExecutionContextLifecycleStateObserver::GetExecutionContext();
}

void PresentationAvailability::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);
  if (event_type == event_type_names::kChange) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationAvailabilityChangeEventListener);
  }
}

void PresentationAvailability::AvailabilityChanged(
    blink::mojom::ScreenAvailability availability) {
  bool value = availability == blink::mojom::ScreenAvailability::AVAILABLE;
  if (value_ == value)
    return;

  value_ = value;
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

bool PresentationAvailability::HasPendingActivity() const {
  return state_ != State::kInactive;
}

void PresentationAvailability::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state == mojom::FrameLifecycleState::kRunning)
    SetState(State::kActive);
  else
    SetState(State::kSuspended);
}

void PresentationAvailability::ContextDestroyed() {
  SetState(State::kInactive);
}

void PresentationAvailability::PageVisibilityChanged() {
  if (state_ == State::kInactive)
    return;
  UpdateListening();
}

void PresentationAvailability::SetState(State state) {
  state_ = state;
  UpdateListening();
}

void PresentationAvailability::UpdateListening() {
  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller)
    return;

  if (state_ == State::kActive &&
      (To<LocalDOMWindow>(GetExecutionContext())->document()->IsPageVisible()))
    controller->GetAvailabilityState()->AddObserver(this);
  else
    controller->GetAvailabilityState()->RemoveObserver(this);
}

const Vector<KURL>& PresentationAvailability::Urls() const {
  return urls_;
}

bool PresentationAvailability::value() const {
  return value_;
}

void PresentationAvailability::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

}  // namespace blink
