// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_grip_space.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source_event.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/modules/xr/xr_target_ray_space.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

namespace {
std::unique_ptr<TransformationMatrix> TryGetTransformationMatrix(
    const base::Optional<gfx::Transform>& transform) {
  if (transform) {
    return std::make_unique<TransformationMatrix>(transform->matrix());
  }

  return nullptr;
}

std::unique_ptr<TransformationMatrix> TryGetTransformationMatrix(
    const TransformationMatrix* other) {
  if (other) {
    return std::make_unique<TransformationMatrix>(*other);
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

  updated_source->UpdateGamepad(state->gamepad);

  // Update the input source's description if this state update includes them.
  if (state->description) {
    const device::mojom::blink::XRInputSourceDescriptionPtr& desc =
        state->description;

    updated_source->state_.target_ray_mode = desc->target_ray_mode;
    updated_source->state_.handedness = desc->handedness;

    updated_source->input_from_pointer_ =
        TryGetTransformationMatrix(desc->input_from_pointer);

    updated_source->state_.profiles.clear();
    for (const auto& name : state->description->profiles) {
      updated_source->state_.profiles.push_back(name);
    }
  }

  updated_source->mojo_from_input_ =
      TryGetTransformationMatrix(state->mojo_from_input);

  updated_source->state_.emulated_position = state->emulated_position;

  return updated_source;
}

XRInputSource::XRInputSource(XRSession* session,
                             uint32_t source_id,
                             device::mojom::XRTargetRayMode target_ray_mode)
    : state_(source_id, target_ray_mode, session->xr()->NavigationStart()),
      session_(session),
      target_ray_space_(MakeGarbageCollected<XRTargetRaySpace>(session, this)),
      grip_space_(MakeGarbageCollected<XRGripSpace>(session, this)) {}

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
      mojo_from_input_(
          TryGetTransformationMatrix(other.mojo_from_input_.get())),
      input_from_pointer_(
          TryGetTransformationMatrix(other.input_from_pointer_.get())) {}

const String XRInputSource::handedness() const {
  switch (state_.handedness) {
    case device::mojom::XRHandedness::NONE:
      return "none";
    case device::mojom::XRHandedness::LEFT:
      return "left";
    case device::mojom::XRHandedness::RIGHT:
      return "right";
  }

  NOTREACHED() << "Unknown handedness: " << state_.handedness;
}

const String XRInputSource::targetRayMode() const {
  switch (state_.target_ray_mode) {
    case device::mojom::XRTargetRayMode::GAZING:
      return "gaze";
    case device::mojom::XRTargetRayMode::POINTING:
      return "tracked-pointer";
    case device::mojom::XRTargetRayMode::TAPPING:
      return "screen";
  }

  NOTREACHED() << "Unknown target ray mode: " << state_.target_ray_mode;
}

XRSpace* XRInputSource::targetRaySpace() const {
  return target_ray_space_;
}

XRSpace* XRInputSource::gripSpace() const {
  if (state_.target_ray_mode == device::mojom::XRTargetRayMode::POINTING) {
    return grip_space_;
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

    if (state->description->profiles.size() != state_.profiles.size()) {
      return true;
    }

    for (wtf_size_t i = 0; i < state_.profiles.size(); ++i) {
      if (state->description->profiles[i] != state_.profiles[i]) {
        return true;
      }
    }
  }

  return false;
}

void XRInputSource::SetInputFromPointer(
    const TransformationMatrix* input_from_pointer) {
  input_from_pointer_ = TryGetTransformationMatrix(input_from_pointer);
}

void XRInputSource::SetGamepadConnected(bool state) {
  if (gamepad_)
    gamepad_->SetConnected(state);
}

void XRInputSource::UpdateGamepad(
    const base::Optional<device::Gamepad>& gamepad) {
  if (gamepad) {
    if (!gamepad_) {
      gamepad_ = MakeGarbageCollected<Gamepad>(this, -1, state_.base_timestamp,
                                               base::TimeTicks::Now());
    }

    gamepad_->UpdateFromDeviceState(*gamepad);
  } else {
    gamepad_ = nullptr;
  }
}

base::Optional<XRNativeOriginInformation> XRInputSource::nativeOrigin() const {
  return XRNativeOriginInformation::Create(this);
}

void XRInputSource::OnSelectStart() {
  // Discard duplicate events and ones after the session has ended.
  if (state_.primary_input_pressed || session_->ended())
    return;

  state_.primary_input_pressed = true;
  state_.selection_cancelled = false;

  XRInputSourceEvent* event =
      CreateInputSourceEvent(event_type_names::kSelectstart);
  session_->DispatchEvent(*event);

  if (event->defaultPrevented())
    state_.selection_cancelled = true;

  // Ensure the frame cannot be used outside of the event handler.
  event->frame()->Deactivate();
}

void XRInputSource::OnSelectEnd() {
  // Discard duplicate events and ones after the session has ended.
  if (!state_.primary_input_pressed || session_->ended())
    return;

  state_.primary_input_pressed = false;

  LocalFrame* frame = session_->xr()->GetFrame();
  if (!frame)
    return;

  XRInputSourceEvent* event =
      CreateInputSourceEvent(event_type_names::kSelectend);
  session_->DispatchEvent(*event);

  if (event->defaultPrevented())
    state_.selection_cancelled = true;

  // Ensure the frame cannot be used outside of the event handler.
  event->frame()->Deactivate();
}

void XRInputSource::OnSelect() {
  // If a select was fired but we had not previously started the selection it
  // indicates a sub-frame or instantaneous select event, and we should fire a
  // selectstart prior to the selectend.
  if (!state_.primary_input_pressed) {
    OnSelectStart();
  }

  LocalFrame* frame = session_->xr()->GetFrame();
  LocalFrame::NotifyUserActivation(frame);

  // If SelectStart caused the session to end, we shouldn't try to fire the
  // select event.
  if (!state_.selection_cancelled && !session_->ended()) {
    if (!frame)
      return;
    XRInputSourceEvent* event =
        CreateInputSourceEvent(event_type_names::kSelect);
    session_->DispatchEvent(*event);

    // Ensure the frame cannot be used outside of the event handler.
    event->frame()->Deactivate();
  }

  OnSelectEnd();
}

void XRInputSource::UpdateSelectState(
    const device::mojom::blink::XRInputSourceStatePtr& state) {
  if (!state)
    return;

  // Handle state change of the primary input, which may fire events
  if (state->primary_input_clicked)
    OnSelect();

  if (state->primary_input_pressed) {
    OnSelectStart();
  } else if (state_.primary_input_pressed) {
    // May get here if the input source was previously pressed but now isn't,
    // but the input source did not set primary_input_clicked to true. We will
    // treat this as a cancelled selection, firing the selectend event so the
    // page stays in sync with the controller state but won't fire the
    // usual select event.
    OnSelectEnd();
  }
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

  SetGamepadConnected(false);
}

XRInputSourceEvent* XRInputSource::CreateInputSourceEvent(
    const AtomicString& type) {
  XRFrame* presentation_frame = session_->CreatePresentationFrame();
  return XRInputSourceEvent::Create(type, presentation_frame, this);
}

void XRInputSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(target_ray_space_);
  visitor->Trace(grip_space_);
  visitor->Trace(gamepad_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
