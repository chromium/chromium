// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/mixed_reality_input_helper.h"

#include <wrl/event.h>

#include <utility>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/util/xr_standard_gamepad_builder.h"
#include "device/vr/windows_mixed_reality/mixed_reality_renderloop.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_location.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_manager.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source_state.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_origins.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_pose.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_source_pose.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_timestamp.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_wrapper_factories.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

// We want to differentiate from gfx::Members, so we're not going to explicitly
// use anything from Windows::Foundation::Numerics
namespace WFN = ABI::Windows::Foundation::Numerics;

using Handedness =
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness;
using PressKind = ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind;
using SourceKind =
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceKind;
using PositionAccuracy =
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourcePositionAccuracy;

MixedRealityInputHelper::ControllerState::ControllerState() = default;
MixedRealityInputHelper::ControllerState::~ControllerState() = default;

namespace {
base::Optional<Gamepad> GetWebXRGamepad(const WMRInputSourceState* source_state,
                                        const mojom::XRHandedness& handedness) {
  XRStandardGamepadBuilder builder(handedness);

  // Add the Select button
  GamepadBuilder::ButtonData data = {};
  data.pressed = source_state->IsSelectPressed();
  data.touched = data.pressed;
  data.value = source_state->SelectPressedValue();
  data.type = GamepadBuilder::ButtonData::Type::kButton;
  builder.SetPrimaryButton(data);

  // Add the grip button
  data = {};
  data.pressed = source_state->IsGrasped();
  data.touched = data.pressed;
  data.value = data.pressed ? 1.0 : 0.0;
  data.type = GamepadBuilder::ButtonData::Type::kButton;
  builder.SetSecondaryButton(data);

  // Select and grip are the only two required buttons, if we can't get the
  // others, we can safely return just them.
  if (!source_state->SupportsControllerProperties())
    return builder.GetGamepad();

  // Add the Thumbstick
  data = {};
  data.pressed = source_state->IsThumbstickPressed();
  data.touched = data.pressed;
  data.value = data.pressed ? 1.0 : 0.0;

  // Invert the y axis because -1 is up in the Gamepad API, but down in WMR.
  data.type = GamepadBuilder::ButtonData::Type::kThumbstick;
  data.x_axis = source_state->ThumbstickX();
  data.y_axis = -source_state->ThumbstickY();
  builder.SetThumbstickData(data);

  // Add the Touchpad
  data = {};
  data.pressed = source_state->IsTouchpadPressed();
  data.touched = source_state->IsTouchpadTouched() || data.pressed;
  data.value = data.pressed ? 1.0 : 0.0;

  // The Touchpad does have Axes, but if it's not touched, they are 0.
  data.type = GamepadBuilder::ButtonData::Type::kTouchpad;
  if (data.touched) {
    // Invert the y axis because -1 is up in the Gamepad API, but down in WMR.
    data.x_axis = source_state->TouchpadX();
    data.y_axis = -source_state->TouchpadY();
  } else {
    data.x_axis = 0;
    data.y_axis = 0;
  }

  builder.SetTouchpadData(data);

  return builder.GetGamepad();
}

gfx::Transform CreateTransform(const WFN::Vector3& position,
                               const WFN::Quaternion& rotation) {
  gfx::DecomposedTransform decomposed_transform;
  decomposed_transform.translate[0] = position.X;
  decomposed_transform.translate[1] = position.Y;
  decomposed_transform.translate[2] = position.Z;

  decomposed_transform.quaternion =
      gfx::Quaternion(rotation.X, rotation.Y, rotation.Z, rotation.W);
  return gfx::ComposeTransform(decomposed_transform);
}

base::Optional<gfx::Transform> TryGetGripFromPointer(
    const WMRInputSourceState* state,
    const WMRInputSource* source,
    const WMRCoordinateSystem* origin,
    gfx::Transform origin_from_grip) {
  if (!origin)
    return base::nullopt;

  // We can get the pointer position, but we'll need to transform it to an
  // offset from the grip position.  If we can't get an inverse of that,
  // then go ahead and bail early.
  gfx::Transform grip_from_origin;
  if (!origin_from_grip.GetInverse(&grip_from_origin))
    return base::nullopt;

  bool pointing_supported = source->IsPointingSupported();

  std::unique_ptr<WMRPointerPose> pointer_pose =
      state->TryGetPointerPose(origin);
  if (!pointer_pose)
    return base::nullopt;

  WFN::Vector3 pos;
  WFN::Quaternion rot;
  if (pointing_supported) {
    std::unique_ptr<WMRPointerSourcePose> pointer_source_pose =
        pointer_pose->TryGetInteractionSourcePose(source);
    if (!pointer_source_pose)
      return base::nullopt;

    pos = pointer_source_pose->Position();
    rot = pointer_source_pose->Orientation();
  } else {
    pos = pointer_pose->HeadForward();
  }

  gfx::Transform origin_from_pointer = CreateTransform(pos, rot);
  return (grip_from_origin * origin_from_pointer);
}

mojom::XRHandedness WindowsToMojoHandedness(Handedness handedness) {
  switch (handedness) {
    case Handedness::SpatialInteractionSourceHandedness_Left:
      return mojom::XRHandedness::LEFT;
    case Handedness::SpatialInteractionSourceHandedness_Right:
      return mojom::XRHandedness::RIGHT;
    default:
      return mojom::XRHandedness::NONE;
  }
}

uint32_t GetSourceId(const WMRInputSource* source) {
  uint32_t id = source->Id();

  // Voice's ID seems to be coming through as 0, which will cause a DCHECK in
  // the hash table used on the blink side.  To ensure that we don't have any
  // collisions with other ids, increment all of the ids by one.
  id++;
  DCHECK_NE(id, 0u);

  return id;
}

const uint16_t kSamsungVendorId = 1118;
const uint16_t kSamsungOdysseyProductId = 1629;

}  // namespace

MixedRealityInputHelper::MixedRealityInputHelper(
    HWND hwnd,
    const base::WeakPtr<MixedRealityRenderLoop>& weak_render_loop)
    : hwnd_(hwnd),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_render_loop_(weak_render_loop) {}

MixedRealityInputHelper::~MixedRealityInputHelper() {
  // Dispose must be called before destruction, which ensures that we're
  // unsubscribed from events.
  DCHECK(pressed_subscription_ == nullptr);
  DCHECK(released_subscription_ == nullptr);
}

void MixedRealityInputHelper::Dispose() {
  UnsubscribeEvents();
}

bool MixedRealityInputHelper::EnsureSpatialInteractionManager() {
  if (input_manager_)
    return true;

  if (!hwnd_)
    return false;

  input_manager_ = WMRInputManagerFactory::GetForWindow(hwnd_);

  if (!input_manager_)
    return false;

  SubscribeEvents();
  return true;
}

std::vector<mojom::XRInputSourceStatePtr>
MixedRealityInputHelper::GetInputState(const WMRCoordinateSystem* origin,
                                       const WMRTimestamp* timestamp) {
  std::vector<mojom::XRInputSourceStatePtr> input_states;

  if (!timestamp || !origin || !EnsureSpatialInteractionManager())
    return input_states;

  auto source_states =
      input_manager_->GetDetectedSourcesAtTimestamp(timestamp->GetRawPtr());

  for (const auto& state : source_states) {
    auto parsed_source_state = ParseWindowsSourceState(state.get(), origin);

    if (parsed_source_state) {
      input_states.push_back(std::move(parsed_source_state));
    }
  }

  return input_states;
}

mojom::XRInputSourceStatePtr MixedRealityInputHelper::ParseWindowsSourceState(
    const WMRInputSourceState* state,
    const WMRCoordinateSystem* origin) {
  if (!origin)
    return nullptr;

  std::unique_ptr<WMRInputSource> source = state->GetSource();
  SourceKind source_kind = source->Kind();

  bool is_controller =
      (source_kind == SourceKind::SpatialInteractionSourceKind_Controller);
  bool is_voice =
      (source_kind == SourceKind::SpatialInteractionSourceKind_Voice);

  if (!(is_controller || is_voice))
    return nullptr;

  // Hands may not have the same id especially if they are lost but since we
  // are only tracking controllers/voice, this id should be consistent.
  uint32_t id = GetSourceId(source.get());

  // Note that if this is untracked we're not supposed to send up the "grip"
  // position, so this will be left as identity and let us still use the same
  // code paths. Any transformations will leave the original item unaffected.
  // Voice input is always untracked.
  gfx::Transform origin_from_grip;
  bool is_tracked = false;
  bool emulated_position = false;
  uint16_t product_id = 0;
  uint16_t vendor_id = 0;
  if (is_controller) {
    std::unique_ptr<WMRInputLocation> location_in_origin =
        state->TryGetLocation(origin);
    if (location_in_origin) {
      WFN::Vector3 pos;
      WFN::Quaternion rot;
      if (location_in_origin->TryGetPosition(&pos) &&
          location_in_origin->TryGetOrientation(&rot)) {
        origin_from_grip = CreateTransform(pos, rot);
        is_tracked = true;
      }

      PositionAccuracy position_accuracy;
      if (location_in_origin->TryGetPositionAccuracy(&position_accuracy) &&
          (position_accuracy ==
           PositionAccuracy::
               SpatialInteractionSourcePositionAccuracy_Approximate)) {
        // Controller lost precise tracking or has its position estimated.
        emulated_position = true;
      }
    }

    std::unique_ptr<WMRController> controller = source->Controller();
    if (controller) {
      product_id = controller->ProductId();
      vendor_id = controller->VendorId();
    }
  }

  base::Optional<gfx::Transform> grip_from_pointer =
      TryGetGripFromPointer(state, source.get(), origin, origin_from_grip);

  // If we failed to get grip_from_pointer, see if it is cached.  If we did get
  // it, update the cache.
  if (!grip_from_pointer) {
    grip_from_pointer = controller_states_[id].grip_from_pointer;
  } else {
    controller_states_[id].grip_from_pointer = grip_from_pointer;
  }

  // Now that we have calculated information for the object, build it.
  mojom::XRInputSourceStatePtr input_state = mojom::XRInputSourceState::New();

  input_state->source_id = id;
  input_state->primary_input_pressed = controller_states_[id].pressed;
  input_state->primary_input_clicked = controller_states_[id].clicked;

  // Grip position should *only* be specified if the controller is tracked.
  if (is_tracked)
    input_state->mojo_from_input = origin_from_grip;

  mojom::XRInputSourceDescriptionPtr description =
      mojom::XRInputSourceDescription::New();

  input_state->emulated_position = emulated_position;
  description->input_from_pointer = grip_from_pointer;

  if (is_voice) {
    description->target_ray_mode = mojom::XRTargetRayMode::GAZING;
    description->handedness = mojom::XRHandedness::NONE;
  } else if (is_controller) {
    description->target_ray_mode = mojom::XRTargetRayMode::POINTING;
    description->handedness = WindowsToMojoHandedness(source->Handedness());

    // If we know the particular headset/controller model, add this to the
    // profiles array.
    if (vendor_id == kSamsungVendorId &&
        product_id == kSamsungOdysseyProductId) {
      description->profiles.push_back("samsung-odyssey");
    }

    description->profiles.push_back("windows-mixed-reality");

    // This makes it clear that the controller actually has a grip button and
    // touchpad and thumbstick input. Otherwise, it's ambiguous whether slots
    // like the touchpad buttons + axes are hooked up vs just placeholders.
    description->profiles.push_back(
        "generic-trigger-squeeze-touchpad-thumbstick");

    input_state->gamepad = GetWebXRGamepad(state, description->handedness);
  } else {
    NOTREACHED();
  }

  input_state->description = std::move(description);
  return input_state;
}

void MixedRealityInputHelper::OnSourcePressed(
    const WMRInputSourceEventArgs& args) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MixedRealityInputHelper::ProcessSourceEvent,
                     weak_ptr_factory_.GetWeakPtr(), args.PressKind(),
                     args.State(), true /* is_pressed */));
}

void MixedRealityInputHelper::OnSourceReleased(
    const WMRInputSourceEventArgs& args) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MixedRealityInputHelper::ProcessSourceEvent,
                     weak_ptr_factory_.GetWeakPtr(), args.PressKind(),
                     args.State(), false /* is_pressed */));
}

void MixedRealityInputHelper::ProcessSourceEvent(
    PressKind press_kind,
    std::unique_ptr<WMRInputSourceState> state,
    bool is_pressed) {
  if (press_kind != PressKind::SpatialInteractionPressKind_Select)
    return;

  std::unique_ptr<WMRInputSource> source = state->GetSource();
  SourceKind source_kind = source->Kind();

  if (source_kind != SourceKind::SpatialInteractionSourceKind_Controller &&
      source_kind != SourceKind::SpatialInteractionSourceKind_Voice)
    return;

  uint32_t id = GetSourceId(source.get());

  bool wasPressed = controller_states_[id].pressed;
  controller_states_[id].pressed = is_pressed;
  controller_states_[id].clicked = (wasPressed && !is_pressed);

  if (!weak_render_loop_)
    return;

  auto* origin = weak_render_loop_->GetOrigin();
  if (!origin)
    return;

  auto parsed_source_state = ParseWindowsSourceState(state.get(), origin);
  if (parsed_source_state) {
    weak_render_loop_->OnInputSourceEvent(std::move(parsed_source_state));
  }

  // We've sent up the click, so clear it.
  controller_states_[id].clicked = false;
}

void MixedRealityInputHelper::SubscribeEvents() {
  DCHECK(input_manager_);
  DCHECK(pressed_subscription_ == nullptr);
  DCHECK(released_subscription_ == nullptr);

  // Unretained is safe since we explicitly get disposed and unsubscribe before
  // destruction
  pressed_subscription_ =
      input_manager_->AddPressedCallback(base::BindRepeating(
          &MixedRealityInputHelper::OnSourcePressed, base::Unretained(this)));
  released_subscription_ =
      input_manager_->AddReleasedCallback(base::BindRepeating(
          &MixedRealityInputHelper::OnSourceReleased, base::Unretained(this)));
}

void MixedRealityInputHelper::UnsubscribeEvents() {
  pressed_subscription_ = nullptr;
  released_subscription_ = nullptr;
}

}  // namespace device
