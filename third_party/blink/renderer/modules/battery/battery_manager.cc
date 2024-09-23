// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/battery/battery_manager.h"

#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/battery/battery_dispatcher.h"

namespace blink {

const char BatteryManager::kSupplementName[] = "BatteryManager";

// static
ScriptPromise<BatteryManager> BatteryManager::getBattery(
    ScriptState* script_state,
    Navigator& navigator) {
  if (!navigator.DomWindow())
    return EmptyPromise();

  // Check to see if this request would be blocked according to the Battery
  // Status API specification.
  LocalDOMWindow* window = navigator.DomWindow();
  // TODO(crbug.com/1007264, crbug.com/1290231): remove fenced frame specific
  // code when permission policy implements the battery status API support.
  if (window->GetFrame()->IsInFencedFrameTree()) {
    return ScriptPromise<BatteryManager>::RejectWithDOMException(
        script_state,
        DOMException::Create(
            "getBattery is not allowed in a fenced frame tree.",
            DOMException::GetErrorName(DOMExceptionCode::kNotAllowedError)));
  }
  window->GetFrame()->CountUseIfFeatureWouldBeBlockedByPermissionsPolicy(
      WebFeature::kBatteryStatusCrossOrigin,
      WebFeature::kBatteryStatusSameOriginABA);

  auto* supplement = Supplement<Navigator>::From<BatteryManager>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<BatteryManager>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement->StartRequest(script_state);
}

BatteryManager::~BatteryManager() = default;

BatteryManager::BatteryManager(Navigator& navigator)
    : ActiveScriptWrappable<BatteryManager>({}),
      Supplement<Navigator>(navigator),
      ExecutionContextLifecycleStateObserver(navigator.DomWindow()),
      PlatformEventController(*navigator.DomWindow()),
      battery_dispatcher_(
          MakeGarbageCollected<BatteryDispatcher>(navigator.DomWindow())) {
  UpdateStateIfNeeded();
}

ScriptPromise<BatteryManager> BatteryManager::StartRequest(
    ScriptState* script_state) {
  if (!battery_property_) {
    battery_property_ = MakeGarbageCollected<BatteryProperty>(
        ExecutionContext::From(script_state));

    // If the context is in a stopped state already, do not start updating.
    if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
      battery_property_->Resolve(this);
    } else {
      has_event_listener_ = true;
      StartUpdating();
    }
  }

  return battery_property_->Promise(script_state->World());
}

bool BatteryManager::charging() {
  return battery_status_.Charging();
}

double BatteryManager::chargingTime() {
  return battery_status_.charging_time().InSecondsF();
}

double BatteryManager::dischargingTime() {
  return battery_status_.discharging_time().InSecondsF();
}

double BatteryManager::level() {
  return battery_status_.Level();
}

void BatteryManager::DidUpdateData() {
  DCHECK(battery_property_);

  BatteryStatus old_status = battery_status_;
  battery_status_ = *battery_dispatcher_->LatestData();

  if (battery_property_->GetState() == BatteryProperty::kPending) {
    battery_property_->Resolve(this);
    return;
  }

  DCHECK(GetExecutionContext());
  if (GetExecutionContext()->IsContextPaused() ||
      GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (battery_status_.Charging() != old_status.Charging())
    DispatchEvent(*Event::Create(event_type_names::kChargingchange));
  if (battery_status_.charging_time() != old_status.charging_time())
    DispatchEvent(*Event::Create(event_type_names::kChargingtimechange));
  if (battery_status_.discharging_time() != old_status.discharging_time())
    DispatchEvent(*Event::Create(event_type_names::kDischargingtimechange));
  if (battery_status_.Level() != old_status.Level())
    DispatchEvent(*Event::Create(event_type_names::kLevelchange));
}

void BatteryManager::RegisterWithDispatcher() {
  battery_dispatcher_->AddController(this, DomWindow());
}

void BatteryManager::UnregisterWithDispatcher() {
  battery_dispatcher_->RemoveController(this);
}

bool BatteryManager::HasLastData() {
  return battery_dispatcher_->LatestData();
}

void BatteryManager::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state == mojom::FrameLifecycleState::kRunning) {
    has_event_listener_ = true;
    StartUpdating();
  } else {
    has_event_listener_ = false;
    StopUpdating();
  }
}

void BatteryManager::ContextDestroyed() {
  has_event_listener_ = false;
  battery_property_ = nullptr;
  StopUpdating();
}

bool BatteryManager::HasPendingActivity() const {
  // Prevent V8 from garbage collecting the wrapper object if there are
  // event listeners or pending promises attached to it.
  return HasEventListeners() ||
         (battery_property_ &&
          battery_property_->GetState() == BatteryProperty::kPending);
}

void BatteryManager::Trace(Visitor* visitor) const {
  visitor->Trace(battery_property_);
  visitor->Trace(battery_dispatcher_);
  Supplement<Navigator>::Trace(visitor);
  PlatformEventController::Trace(visitor);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

}  // namespace blink
