// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"

#include "base/time/time.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_handedness.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_target_ray_mode.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/modules/xr/xr_grip_space.h"
#include "third_party/blink/renderer/modules/xr/xr_hand.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source_event.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_session_event.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/modules/xr/xr_target_ray_space.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

namespace {
std::unique_ptr<gfx::Transform> TryGetTransform(
    const std::optional<gfx::Transform>& transform) {
  if (transform) {
    return std::make_unique<gfx::Transform>(*transform);
  }

  return nullptr;
}

std::unique_ptr<gfx::Transform> TryGetTransform(const gfx::Transform* other) {
  if (other) {
    return std::make_unique<gfx::Transform>(*other);
  }

  return nullptr;
}
}  // namespace

XRInputSource::InternalState::InternalState(
    uint32_t source_id,
    device::mojom::XRTargetRayMode target_ray_mode,
    base::TimeTicks base_timestamp)
    : source_id(source_id),
      target_ray_mode(target_ray_mode),
      base_timestamp(base_timestamp) {}

XRInputSource::InternalState::InternalState(const InternalState& other) =
    default;

XRInputSource::InternalState::~InternalState() = default;

// static
XRInputSource* XRInputSource::CreateOrUpdateFrom(
    XRInputSource* other,
    XRSession* session,
    const device::mojom::blink::XRInputSourceStatePtr& state) {
  if (!state)
    return other;

  XRInputSource* updated_source = other;

  // Check if we have an existing object, and if we do, if it can be re-used.
  if (!other) {
    auto source_id = state->source_id;
    updated_source = MakeGarbageCollected<XRInputSource>(session, source_id);
  } else if (other->InvalidatesSameObject(state)) {
    // Something in the state has changed which requires us to re-create the
    // object.  Create a copy now, and we will blindly update any state later,
    // knowing that we now have a new object if needed.
    updated_source = MakeGarbageCollected<XRInputSource>(*other);
  }

  if (updated_source->state_.is_visible) {
    updated_source->UpdateGamepad(state->gamepad);
  }

  // Update the input source's description if this state update includes them.
  if (state->description) {
    const device::mojom::blink::XRInputSourceDescriptionPtr& desc =
        state->description;

    updated_source->state_.target_ray_mode = desc->target_ray_mode;
    updated_source->state_.handedness = desc->handedness;

    if (updated_source->state_.is_visible) {
      updated_source->input_from_pointer_ =
          TryGetTransform(desc->input_from_pointer);
    }

    updated_source->profiles_ = MakeGarbageCollected<FrozenArray<IDLString>>(
        state->description->profiles);
  }

  if (updated_source->state_.is_visible) {
    updated_source->mojo_from_input_ = TryGetTransform(state->mojo_from_input);
  }

  if (updated_source->state_.is_visible) {
    updated_source->UpdateHand(state->hand_tracking_data.get());
  }

  updated_source->state_.emulated_position = state->emulated_position;

  return updated_source;
}

XRInputSource::XRInputSource(XRSession* session,
                             uint32_t source_id,
                             device::mojom::XRTargetRayMode target_ray_mode)
    : state_(source_id, target_ray_mode, session->xr()->NavigationStart()),
      session_(session),
      target_ray_space_(MakeGarbageCollected<XRTargetRaySpace>(session, this)),
      grip_space_(MakeGarbageCollected<XRGripSpace>(session, this)),
      profiles_(MakeGarbageCollected<FrozenArray<IDLString>>()) {}

// Must make new target_ray_space_ and grip_space_ to ensure that they point to
// the correct XRInputSource object. Otherwise, the controller position gets
// stuck when an XRInputSource gets re-created. Also need to make a deep copy of
// the matrices since they use unique_ptrs.
XRInputSource::XRInputSource(const XRInputSource& other)
    : state_(other.state_),
      session_(other.session_),
      target_ray_space_(
          MakeGarbageCollected<XRTargetRaySpace>(other.session_, this)),
      grip_space_(MakeGarbageCollected<XRGripSpace>(other.session_, this)),
      gamepad_(other.gamepad_),
      hand_(other.hand_),
      profiles_(MakeGarbageCollected<FrozenArray<IDLString>>(
          other.profiles_->AsVector())),
      mojo_from_input_(TryGetTransform(other.mojo_from_input_.get())),
      input_from_pointer_(TryGetTransform(other.input_from_pointer_.get())) {}

V8XRHandedness XRInputSource::handedness() const {
  switch (state_.handedness) {
    case device::mojom::XRHandedness::NONE:
      return V8XRHandedness(V8XRHandedness::Enum::kNone);
    case device::mojom::XRHandedness::LEFT:
      return V8XRHandedness(V8XRHandedness::Enum::kLeft);
    case device::mojom::XRHandedness::RIGHT:
      return V8XRHandedness(V8XRHandedness::Enum::kRight);
  }

  NOTREACHED();
}

V8XRTargetRayMode XRInputSource::targetRayMode() const {
  switch (state_.target_ray_mode) {
    case device::mojom::XRTargetRayMode::GAZING:
      return V8XRTargetRayMode(V8XRTargetRayMode::Enum::kGaze);
    case device::mojom::XRTargetRayMode::POINTING:
      return V8XRTargetRayMode(V8XRTargetRayMode::Enum::kTrackedPointer);
    case device::mojom::XRTargetRayMode::TAPPING:
      return V8XRTargetRayMode(V8XRTargetRayMode::Enum::kScreen);
  }
  NOTREACHED();
}

XRSpace* XRInputSource::targetRaySpace() const {
  return target_ray_space_.Get();
}

XRSpace* XRInputSource::gripSpace() const {
  if (!state_.is_visible)
    return nullptr;

  if (state_.target_ray_mode == device::mojom::XRTargetRayMode::POINTING) {
    return grip_space_.Get();
  }

  return nullptr;
}

bool XRInputSource::InvalidatesSameObject(
    const device::mojom::blink::XRInputSourceStatePtr& state) {
  if ((state->gamepad && !gamepad_) || (!state->gamepad && gamepad_)) {
    return true;
  }

  if (state->description) {
    if (state->description->handedness != state_.handedness) {
      return true;
    }

    if (state->description->target_ray_mode != state_.target_ray_mode) {
      return true;
    }

    if (state->description->profiles.size() != profiles_->size()) {
      return true;
    }

    for (wtf_size_t i = 0; i < profiles_->size(); ++i) {
      if (state->description->profiles[i] != (*profiles_)[i]) {
        return true;
      }
    }
  }

  if ((state->hand_tracking_data.get() && !hand_) ||
      (!state->hand_tracking_data.get() && hand_)) {
    return true;
  }

  return false;
}

void XRInputSource::SetInputFromPointer(
    const gfx::Transform* input_from_pointer) {
  if (state_.is_visible) {
    input_from_pointer_ = TryGetTransform(input_from_pointer);
  }
}

void XRInputSource::SetGamepadConnected(bool state) {
  if (gamepad_)
    gamepad_->SetConnected(state);
}

void XRInputSource::UpdateGamepad(
    const std::optional<device::Gamepad>& gamepad) {
  if (gamepad) {
    if (!gamepad_) {
      gamepad_ = MakeGarbageCollected<Gamepad>(this, -1, state_.base_timestamp,
                                               base::TimeTicks::Now());
    }

    LocalDOMWindow* window = session_->xr()->DomWindow();
    bool cross_origin_isolated_capability =
        window ? window->CrossOriginIsolatedCapability() : false;
    gamepad_->UpdateFromDeviceState(*gamepad, cross_origin_isolated_capability);
  } else {
    gamepad_ = nullptr;
  }
}

void XRInputSource::UpdateHand(
    const device::mojom::blink::XRHandTrackingData* hand_tracking_data) {
  if (hand_tracking_data) {
    if (!hand_) {
      hand_ = MakeGarbageCollected<XRHand>(hand_tracking_data, this);
    } else {
      hand_->updateFromHandTrackingData(hand_tracking_data, this);
    }
  } else {
    hand_ = nullptr;
  }
}

std::optional<gfx::Transform> XRInputSource::MojoFromInput() const {
  if (!mojo_from_input_.get()) {
    return std::nullopt;
  }
  return *(mojo_from_input_.get());
}

std::optional<gfx::Transform> XRInputSource::InputFromPointer() const {
  if (!input_from_pointer_.get()) {
    return std::nullopt;
  }
  return *(input_from_pointer_.get());
}

void XRInputSource::OnSelectStart() {
  DVLOG(3) << __func__;
  // Discard duplicate events and ones after the session has ended.
  if (state_.primary_input_pressed || session_->ended())
    return;

  state_.primary_input_pressed = true;
  state_.selection_cancelled = false;

  DVLOG(3) << __func__ << ": dispatch selectstart event";
  XRInputSourceEvent* event =
      CreateInputSourceEvent(event_type_names::kSelectstart);
  session_->DispatchEvent(*event);

  if (event->defaultPrevented())
    state_.selection_cancelled = true;

  // Ensure the frame cannot be used outside of the event handler.
  event->frame()->Deactivate();
}

void XRInputSource::OnSelectEnd() {
  DVLOG(3) << __func__;
  // Discard duplicate events and ones after the session has ended.
  if (!state_.primary_input_pressed || session_->ended())
    return;

  state_.primary_input_pressed = false;

  if (!session_->xr()->DomWindow())
    return;

  DVLOG(3) << __func__ << ": dispatch selectend event";
  XRInputSourceEvent* event =
      CreateInputSourceEvent(event_type_names::kSelectend);
  session_->DispatchEvent(*event);

  if (event->defaultPrevented())
    state_.selection_cancelled = true;

  // Ensure the frame cannot be used outside of the event handler.
  event->frame()->Deactivate();
}

void XRInputSource::OnSelect() {
  DVLOG(3) << __func__;
  // If a select was fired but we had not previously started the selection it
  // indicates a sub-frame or instantaneous select event, and we should fire a
  // selectstart prior to the selectend.
  if (!state_.primary_input_pressed) {
    OnSelectStart();
  }

  // If SelectStart caused the session to end, we shouldn't try to fire the
  // select event.
  LocalDOMWindow* window = session_->xr()->DomWindow();
  if (!window)
    return;
  LocalFrame::NotifyUserActivation(
      window->GetFrame(),
      mojom::blink::UserActivationNotificationType::kInteraction);

  if (!state_.selection_cancelled && !session_->ended()) {
    DVLOG(3) << __func__ << ": dispatch select event";
    XRInputSourceEvent* event =
        CreateInputSourceEvent(event_type_names::kSelect);
    session_->DispatchEvent(*event);

    // Ensure the frame cannot be used outside of the event handler.
    event->frame()->Deactivate();
  }

  OnSelectEnd();
}

void XRInputSource::OnSqueezeStart() {
  DVLOG(3) << __func__;
  // Discard duplicate events and ones after the session has ended.
  if (state_.primary_squeeze_pressed || session_->ended())
    return;

  state_.primary_squeeze_pressed = true;
  state_.squeezing_cancelled = false;

  XRInputSourceEvent* event =
      CreateInputSourceEvent(event_type_names::kSqueezestart);
  session_->DispatchEvent(*event);

  if (event->defaultPrevented())
    state_.squeezing_cancelled = true;

  // Ensure the frame cannot be used outside of the event handler.
  event->frame()->Deactivate();
}

void XRInputSource::OnSqueezeEnd() {
  DVLOG(3) << __func__;
  // Discard duplicate events and ones after the session has ended.
  if (!state_.primary_squeeze_pressed || session_->ended())
    return;

  state_.primary_squeeze_pressed = false;

  if (!session_->xr()->DomWindow())
    return;

  DVLOG(3) << __func__ << ": dispatch squeezeend event";
  XRInputSourceEvent* event =
      CreateInputSourceEvent(event_type_names::kSqueezeend);
  session_->DispatchEvent(*event);

  if (event->defaultPrevented())
    state_.squeezing_cancelled = true;

  // Ensure the frame cannot be used outside of the event handler.
  event->frame()->Deactivate();
}

void XRInputSource::OnSqueeze() {
  DVLOG(3) << __func__;
  // If a squeeze was fired but we had not previously started the squeezing it
  // indicates a sub-frame or instantaneous squeeze event, and we should fire a
  // squeezestart prior to the squeezeend.
  if (!state_.primary_squeeze_pressed) {
    OnSqueezeStart();
  }

  // If SelectStart caused the session to end, we shouldn't try to fire the
  // select event.
  LocalDOMWindow* window = session_->xr()->DomWindow();
  if (!window)
    return;
  LocalFrame::NotifyUserActivation(
      window->GetFrame(),
      mojom::blink::UserActivationNotificationType::kInteraction);

  // If SelectStart caused the session to end, we shouldn't try to fire the
  // select event.
  if (!state_.squeezing_cancelled && !session_->ended()) {
    DVLOG(3) << __func__ << ": dispatch squeeze event";
    XRInputSourceEvent* event =
        CreateInputSourceEvent(event_type_names::kSqueeze);
    session_->DispatchEvent(*event);

    // Ensure the frame cannot be used outside of the event handler.
    event->frame()->Deactivate();
  }

  OnSqueezeEnd();
}

void XRInputSource::UpdateButtonStates(
    const device::mojom::blink::XRInputSourceStatePtr& new_state) {
  if (!new_state)
    return;

  DVLOG(3) << __func__ << ": state_.is_visible=" << state_.is_visible
           << ", state_.xr_select_events_suppressed="
           << state_.xr_select_events_suppressed
           << ", new_state->primary_input_clicked="
           << new_state->primary_input_clicked;

  if (!state_.is_visible) {
    DVLOG(3) << __func__ << ": input NOT VISIBLE";
    if (new_state->primary_input_clicked) {
      DVLOG(3) << __func__ << ": got click while invisible, SUPPRESS end";
      state_.xr_select_events_suppressed = false;
    }
    return;
  }
  if (state_.xr_select_events_suppressed) {
    if (new_state->primary_input_clicked) {
      DVLOG(3) << __func__ << ": got click, SUPPRESS end";
      state_.xr_select_events_suppressed = false;
    }
    DVLOG(3) << __func__ << ": overlay input select SUPPRESSED";
    return;
  }

  DCHECK(!state_.xr_select_events_suppressed);

  // Handle state change of the primary input, which may fire events
  if (new_state->primary_input_clicked)
    OnSelect();

  if (new_state->primary_input_pressed) {
    OnSelectStart();
  } else if (state_.primary_input_pressed) {
    // May get here if the input source was previously pressed but now isn't,
    // but the input source did not set primary_input_clicked to true. We will
    // treat this as a cancelled selection, firing the selectend event so the
    // page stays in sync with the controller state but won't fire the
    // usual select event.
    OnSelectEnd();
  }

  // Handle state change of the primary input, which may fire events
  if (new_state->primary_squeeze_clicked)
    OnSqueeze();

  if (new_state->primary_squeeze_pressed) {
    OnSqueezeStart();
  } else if (state_.primary_squeeze_pressed) {
    // May get here if the input source was previously pressed but now isn't,
    // but the input source did not set primary_squeeze_clicked to true. We will
    // treat this as a cancelled squeezeing, firing the squeezeend event so the
    // page stays in sync with the controller state but won't fire the
    // usual squeeze event.
    OnSqueezeEnd();
  }
}

void XRInputSource::ProcessOverlayHitTest(
    Element* overlay_element,
    const device::mojom::blink::XRInputSourceStatePtr& new_state) {
  DVLOG(3) << __func__ << ": state_.xr_select_events_suppressed="
           << state_.xr_select_events_suppressed;

  DCHECK(overlay_element);
  DCHECK(new_state->overlay_pointer_position);

  // Do a hit test at the overlay pointer position to see if the pointer
  // intersects a cross origin iframe. If yes, set the visibility to false which
  // causes targetRaySpace and gripSpace to return null poses.
  gfx::PointF point(new_state->overlay_pointer_position->x(),
                    new_state->overlay_pointer_position->y());
  DVLOG(3) << __func__ << ": hit test point=" << point.ToString();

  HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kTouchEvent |
                                                HitTestRequest::kReadOnly |
                                                HitTestRequest::kActive;

  HitTestResult result = event_handling_util::HitTestResultInFrame(
      overlay_element->GetDocument().GetFrame(), HitTestLocation(point),
      hit_type);
  DVLOG(3) << __func__ << ": hit test InnerElement=" << result.InnerElement();

  Element* hit_element = result.InnerElement();
  if (!hit_element) {
    return;
  }

  // Check if the hit element is cross-origin content. In addition to an iframe,
  // this could potentially be an old-style frame in a frameset, so check for
  // the common base class to cover both. (There's no intention to actively
  // support framesets for DOM Overlay, but this helps prevent them from
  // being used as a mechanism for information leaks.)
  HTMLFrameElementBase* frame = DynamicTo<HTMLFrameElementBase>(hit_element);
  if (frame) {
    Document* hit_document = frame->contentDocument();
    if (hit_document) {
      Frame* hit_frame = hit_document->GetFrame();
      DCHECK(hit_frame);
      if (hit_frame->IsCrossOriginToOutermostMainFrame()) {
        // Mark the input source as invisible until the primary button is
        // released.
        state_.is_visible = false;

        // If this is the first touch, also suppress events, even if it
        // ends up being released outside the frame later.
        if (!state_.primary_input_pressed) {
          state_.xr_select_events_suppressed = true;
        }

        DVLOG(3)
            << __func__
            << ": input source overlaps with cross origin content, is_visible="
            << state_.is_visible << ", xr_select_events_suppressed="
            << state_.xr_select_events_suppressed;
        return;
      }
    }
  }

  // If we get here, the touch didn't hit a cross origin frame. Set the
  // controller spaces visible.
  state_.is_visible = true;

  // Now that the visibility check has finished, mark non-primary input sources
  // as suppressed.
  if (new_state->is_auxiliary) {
    state_.xr_select_events_suppressed = true;
  }

  // Now check if this is a new primary button press. If yes, send a
  // beforexrselect event to give the application an opportunity to cancel the
  // XR input "select" sequence that would normally be caused by this.

  if (state_.xr_select_events_suppressed) {
    DVLOG(3) << __func__ << ": using overlay input provider: SUPPRESS ongoing";
    return;
  }

  if (state_.primary_input_pressed) {
    DVLOG(3) << __func__ << ": ongoing press, not checking again";
    return;
  }

  bool is_primary_press =
      new_state->primary_input_pressed || new_state->primary_input_clicked;
  if (!is_primary_press) {
    DVLOG(3) << __func__ << ": no button press, ignoring";
    return;
  }

  // The event needs to be cancelable (obviously), bubble (so that parent
  // elements can handle it), and composed (so that it crosses shadow DOM
  // boundaries, including UA-added shadow DOM).
  Event* event = MakeGarbageCollected<XRSessionEvent>(
      event_type_names::kBeforexrselect, session_, Event::Bubbles::kYes,
      Event::Cancelable::kYes, Event::ComposedMode::kComposed);

  hit_element->DispatchEvent(*event);
  bool default_prevented = event->defaultPrevented();

  // Keep the input source visible, so it's exposed in the input sources array,
  // but don't generate XR select events for the current button sequence.
  state_.xr_select_events_suppressed = default_prevented;
  DVLOG(3) << __func__ << ": state_.xr_select_events_suppressed="
           << state_.xr_select_events_suppressed;
}

void XRInputSource::OnRemoved() {
  if (state_.primary_input_pressed) {
    state_.primary_input_pressed = false;

    XRInputSourceEvent* event =
        CreateInputSourceEvent(event_type_names::kSelectend);
    session_->DispatchEvent(*event);

    if (event->defaultPrevented())
      state_.selection_cancelled = true;

    // Ensure the frame cannot be used outside of the event handler.
    event->frame()->Deactivate();
  }

  if (state_.primary_squeeze_pressed) {
    state_.primary_squeeze_pressed = false;

    XRInputSourceEvent* event =
        CreateInputSourceEvent(event_type_names::kSqueezeend);
    session_->DispatchEvent(*event);

    if (event->defaultPrevented())
      state_.squeezing_cancelled = true;

    // Ensure the frame cannot be used outside of the event handler.
    event->frame()->Deactivate();
  }

  SetGamepadConnected(false);
}

XRInputSourceEvent* XRInputSource::CreateInputSourceEvent(
    const AtomicString& type) {
  XRFrame* presentation_frame = session_->CreatePresentationFrame();
  return XRInputSourceEvent::Create(type, presentation_frame, this);
}

void XRInputSource::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(target_ray_space_);
  visitor->Trace(grip_space_);
  visitor->Trace(gamepad_);
  visitor->Trace(hand_);
  visitor->Trace(profiles_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
