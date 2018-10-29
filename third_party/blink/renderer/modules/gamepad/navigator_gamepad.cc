/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/gamepad/navigator_gamepad.h"

#include "device/gamepad/public/cpp/gamepad.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_dispatcher.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_event.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_list.h"
#include "third_party/blink/renderer/modules/vr/navigator_vr.h"

namespace blink {

namespace {

// A button press must have a value at least this large to qualify as a user
// activation. The selected value should be greater than 0.5 so that axes
// incorrectly mapped as triggers do not generate activations in the idle
// position.
const double kButtonActivationThreshold = 0.9;

void HasGamepadConnectionChanged(const String& old_id,
                                 const String& new_id,
                                 bool old_connected,
                                 bool new_connected,
                                 bool* gamepad_found,
                                 bool* gamepad_lost) {
  // If the gamepad ID changes, treat it as a disconnection and connection.
  bool id_changed = old_connected && new_connected && old_id != new_id;

  if (gamepad_found)
    *gamepad_found = id_changed || (!old_connected && new_connected);
  if (gamepad_lost)
    *gamepad_lost = id_changed || (old_connected && !new_connected);
}

bool HasUserActivation(GamepadList* gamepads) {
  if (!gamepads)
    return false;
  // A button press counts as a user activation if the button's value is greater
  // than the activation threshold. A threshold is used so that analog buttons
  // or triggers do not generate an activation from a light touch.
  for (wtf_size_t pad_index = 0; pad_index < gamepads->length(); ++pad_index) {
    Gamepad* pad = gamepads->item(pad_index);
    if (pad) {
      const GamepadButtonVector& buttons = pad->buttons();
      for (auto button : buttons) {
        double value = button->value();
        if (value > kButtonActivationThreshold)
          return true;
      }
    }
  }
  return false;
}

}  // namespace

template <typename T>
static void SampleGamepad(unsigned index,
                          T& gamepad,
                          const device::Gamepad& device_gamepad,
                          const TimeTicks& navigation_start) {
  String old_id = gamepad.id();
  bool old_was_connected = gamepad.connected();

  TimeTicks last_updated =
      TimeTicks() + TimeDelta::FromMicroseconds(device_gamepad.timestamp);
  DOMHighResTimeStamp timestamp =
      Performance::MonotonicTimeToDOMHighResTimeStamp(navigation_start,
                                                      last_updated, false);
  gamepad.SetId(device_gamepad.id);
  gamepad.SetConnected(device_gamepad.connected);
  gamepad.SetTimestamp(timestamp);
  gamepad.SetAxes(device_gamepad.axes_length, device_gamepad.axes);
  gamepad.SetButtons(device_gamepad.buttons_length, device_gamepad.buttons);
  gamepad.SetPose(device_gamepad.pose);
  gamepad.SetHand(device_gamepad.hand);

  if (device_gamepad.is_xr) {
    TimeTicks now = TimeTicks::Now();
    TRACE_COUNTER1("input", "XR gamepad pose age (ms)",
                   (now - last_updated).InMilliseconds());
  }

  bool newly_connected;
  HasGamepadConnectionChanged(old_id, gamepad.id(), old_was_connected,
                              gamepad.connected(), &newly_connected, nullptr);

  // These fields are not expected to change and will only be written when the
  // gamepad is newly connected.
  if (newly_connected) {
    gamepad.SetIndex(index);
    gamepad.SetMapping(device_gamepad.mapping);
    gamepad.SetVibrationActuator(device_gamepad.vibration_actuator);
    // Re-map display ids, since we will hand out at most one VRDisplay.
    gamepad.SetDisplayId(device_gamepad.display_id ? 1 : 0);
  } else if (!gamepad.vibrationActuator() &&
             device_gamepad.vibration_actuator.not_null) {
    // Some gamepads require additional steps to determine haptics capability.
    // These gamepads may initially set |vibration_actuator| to null and then
    // update it some time later. Make sure such devices can correctly propagate
    // the changed capabilities.
    gamepad.SetVibrationActuator(device_gamepad.vibration_actuator);
  }
}

template <typename GamepadType, typename ListType>
static void SampleGamepads(ListType* into,
                           const ExecutionContext* context,
                           const TimeTicks& navigation_start) {
  device::Gamepads gamepads;

  GamepadDispatcher::Instance().SampleGamepads(gamepads);

  for (unsigned i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
    device::Gamepad& web_gamepad = gamepads.items[i];

    bool hide_xr_gamepad = false;
    if (web_gamepad.is_xr) {
      bool webxr_enabled =
          (context && OriginTrials::WebXRGamepadSupportEnabled(context) &&
           OriginTrials::WebXREnabled(context));
      bool webvr_enabled = (context && OriginTrials::WebVREnabled(context));

      if (!webxr_enabled && !webvr_enabled) {
        // If neither WebXR nor WebVR are enabled, we should not expose XR-
        // backed gamepads.
        hide_xr_gamepad = true;
      }
    }

    if (hide_xr_gamepad) {
      into->Set(i, nullptr);
    } else if (web_gamepad.connected) {
      GamepadType* gamepad = into->item(i);
      if (!gamepad)
        gamepad = GamepadType::Create();
      SampleGamepad(i, *gamepad, web_gamepad, navigation_start);
      into->Set(i, gamepad);
    } else {
      into->Set(i, nullptr);
    }
  }
}

// static
const char NavigatorGamepad::kSupplementName[] = "NavigatorGamepad";

NavigatorGamepad* NavigatorGamepad::From(Document& document) {
  if (!document.GetFrame() || !document.GetFrame()->DomWindow())
    return nullptr;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return &From(navigator);
}

NavigatorGamepad& NavigatorGamepad::From(Navigator& navigator) {
  NavigatorGamepad* supplement =
      Supplement<Navigator>::From<NavigatorGamepad>(navigator);
  if (!supplement) {
    supplement = new NavigatorGamepad(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
GamepadList* NavigatorGamepad::getGamepads(Navigator& navigator) {
  return NavigatorGamepad::From(navigator).Gamepads();
}

GamepadList* NavigatorGamepad::Gamepads() {
  // Tell VR that gamepad is in use.
  Document* document = GetFrame() ? GetFrame()->GetDocument() : nullptr;
  if (document) {
    NavigatorVR* navigator_vr = NavigatorVR::From(*document);
    if (navigator_vr) {
      navigator_vr->SetDidUseGamepad();
    }
  }

  SampleAndCheckConnectedGamepads();

  // Allow gamepad button presses to qualify as user activations if the page is
  // visible.
  if (RuntimeEnabledFeatures::UserActivationV2Enabled() && GetFrame() &&
      GetPage() && GetPage()->IsPageVisible() && HasUserActivation(gamepads_)) {
    LocalFrame::NotifyUserActivation(GetFrame(), UserGestureToken::kNewGesture);
  }

  return gamepads_.Get();
}

void NavigatorGamepad::Trace(blink::Visitor* visitor) {
  visitor->Trace(gamepads_);
  visitor->Trace(gamepads_back_);
  visitor->Trace(pending_events_);
  visitor->Trace(dispatch_one_event_runner_);
  Supplement<Navigator>::Trace(visitor);
  DOMWindowClient::Trace(visitor);
  PlatformEventController::Trace(visitor);
}

bool NavigatorGamepad::StartUpdatingIfAttached() {
  // The frame must be attached to start updating.
  if (GetFrame()) {
    StartUpdating();
    return true;
  }
  return false;
}

void NavigatorGamepad::DidUpdateData() {
  // We should stop listening once we detached.
  DCHECK(GetFrame());
  DCHECK(DomWindow());

  // We register to the dispatcher before sampling gamepads so we need to check
  // if we actually have an event listener.
  if (!has_event_listener_)
    return;

  SampleAndCheckConnectedGamepads();
}

void NavigatorGamepad::DispatchOneEvent() {
  DCHECK(DomWindow());
  DCHECK(!pending_events_.IsEmpty());

  Gamepad* gamepad = pending_events_.TakeFirst();
  const AtomicString& event_name = gamepad->connected()
                                       ? EventTypeNames::gamepadconnected
                                       : EventTypeNames::gamepaddisconnected;
  DomWindow()->DispatchEvent(*GamepadEvent::Create(
      event_name, Event::Bubbles::kNo, Event::Cancelable::kYes, gamepad));

  if (!pending_events_.IsEmpty()) {
    DCHECK(dispatch_one_event_runner_);
    dispatch_one_event_runner_->RunAsync();
  }
}

NavigatorGamepad::NavigatorGamepad(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      DOMWindowClient(navigator.DomWindow()),
      PlatformEventController(
          navigator.GetFrame() ? navigator.GetFrame()->GetDocument() : nullptr),
      dispatch_one_event_runner_(
          navigator.GetFrame() ? AsyncMethodRunner<NavigatorGamepad>::Create(
                                     this,
                                     &NavigatorGamepad::DispatchOneEvent,
                                     navigator.GetFrame()->GetTaskRunner(
                                         TaskType::kMiscPlatformAPI))
                               : nullptr) {
  if (navigator.DomWindow())
    navigator.DomWindow()->RegisterEventListenerObserver(this);

  // Fetch |window.performance.timing.navigationStart|. Gamepad timestamps are
  // reported relative to this value.
  if (GetFrame()) {
    DocumentLoader* loader = GetFrame()->Loader().GetDocumentLoader();
    if (loader)
      navigation_start_ = loader->GetTiming().NavigationStart();
  }
}

NavigatorGamepad::~NavigatorGamepad() = default;

void NavigatorGamepad::RegisterWithDispatcher() {
  GamepadDispatcher::Instance().AddController(this);
  if (dispatch_one_event_runner_)
    dispatch_one_event_runner_->Unpause();
}

void NavigatorGamepad::UnregisterWithDispatcher() {
  if (dispatch_one_event_runner_)
    dispatch_one_event_runner_->Pause();
  GamepadDispatcher::Instance().RemoveController(this);
}

bool NavigatorGamepad::HasLastData() {
  // Gamepad data is polled instead of pushed.
  return false;
}

static bool IsGamepadEvent(const AtomicString& event_type) {
  return event_type == EventTypeNames::gamepadconnected ||
         event_type == EventTypeNames::gamepaddisconnected;
}

void NavigatorGamepad::DidAddEventListener(LocalDOMWindow*,
                                           const AtomicString& event_type) {
  if (!IsGamepadEvent(event_type))
    return;

  bool first_event_listener = !has_event_listener_;
  has_event_listener_ = true;

  if (GetPage() && GetPage()->IsPageVisible()) {
    StartUpdatingIfAttached();
    if (first_event_listener)
      SampleAndCheckConnectedGamepads();
  }
}

void NavigatorGamepad::DidRemoveEventListener(LocalDOMWindow* window,
                                              const AtomicString& event_type) {
  if (IsGamepadEvent(event_type) &&
      !window->HasEventListeners(EventTypeNames::gamepadconnected) &&
      !window->HasEventListeners(EventTypeNames::gamepaddisconnected)) {
    DidRemoveGamepadEventListeners();
  }
}

void NavigatorGamepad::DidRemoveAllEventListeners(LocalDOMWindow*) {
  DidRemoveGamepadEventListeners();
}

void NavigatorGamepad::DidRemoveGamepadEventListeners() {
  has_event_listener_ = false;
  if (dispatch_one_event_runner_)
    dispatch_one_event_runner_->Stop();
  pending_events_.clear();
  StopUpdating();
}

void NavigatorGamepad::SampleAndCheckConnectedGamepads() {
  ExecutionContext* execution_context =
      DomWindow() ? DomWindow()->GetExecutionContext() : nullptr;

  if (StartUpdatingIfAttached()) {
    if (!gamepads_)
      gamepads_ = GamepadList::Create();
    if (GetPage()->IsPageVisible() && has_event_listener_) {
      if (!gamepads_back_)
        gamepads_back_ = GamepadList::Create();

      // Compare the current sample with the old data and enqueue connection
      // events for any differences.
      SampleGamepads<Gamepad>(gamepads_back_.Get(), execution_context,
                              navigation_start_);
      if (CheckConnectedGamepads(gamepads_.Get(), gamepads_back_.Get())) {
        // If we had any disconnected gamepads, we can't overwrite gamepads_
        // because the Gamepad object from the old buffer is reused as the
        // disconnection event and will be overwritten with new data. Instead,
        // recreate the buffer.
        gamepads_ = GamepadList::Create();
      }
      if (!pending_events_.IsEmpty()) {
        DCHECK(dispatch_one_event_runner_);
        dispatch_one_event_runner_->RunAsync();
      }
    }
    SampleGamepads<Gamepad>(gamepads_.Get(), execution_context,
                            navigation_start_);
  }
}

bool NavigatorGamepad::CheckConnectedGamepads(GamepadList* old_gamepads,
                                              GamepadList* new_gamepads) {
  int disconnection_count = 0;
  for (unsigned i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
    Gamepad* old_gamepad = old_gamepads ? old_gamepads->item(i) : nullptr;
    Gamepad* new_gamepad = new_gamepads->item(i);
    bool connected, disconnected;
    CheckConnectedGamepad(old_gamepad, new_gamepad, &connected, &disconnected);

    if (disconnected) {
      old_gamepad->SetConnected(false);
      pending_events_.push_back(old_gamepad);
      disconnection_count++;
    }
    if (connected) {
      pending_events_.push_back(new_gamepad);
    }
  }
  return disconnection_count > 0;
}

void NavigatorGamepad::CheckConnectedGamepad(Gamepad* old_gamepad,
                                             Gamepad* new_gamepad,
                                             bool* gamepad_found,
                                             bool* gamepad_lost) {
  bool old_connected = old_gamepad && old_gamepad->connected();
  bool new_connected = new_gamepad && new_gamepad->connected();
  if (old_gamepad && new_gamepad) {
    HasGamepadConnectionChanged(old_gamepad->id(), new_gamepad->id(),
                                old_connected, new_connected, gamepad_found,
                                gamepad_lost);
    return;
  }

  if (gamepad_found)
    *gamepad_found = new_connected;
  if (gamepad_lost)
    *gamepad_lost = old_connected;
}

void NavigatorGamepad::PageVisibilityChanged() {
  // Inform the embedder whether it needs to provide gamepad data for us.
  bool visible = GetPage()->IsPageVisible();
  if (visible && (has_event_listener_ || gamepads_)) {
    StartUpdatingIfAttached();
  } else {
    StopUpdating();
  }

  if (visible && has_event_listener_)
    SampleAndCheckConnectedGamepads();
}

}  // namespace blink
