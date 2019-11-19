// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_render_loop.h"

#include "base/trace_event/trace_event.h"
#include "device/vr/openvr/openvr_api_wrapper.h"
#include "device/vr/openvr/openvr_gamepad_helper.h"
#include "device/vr/openvr/openvr_type_converters.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/transform.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace device {

namespace {

// OpenVR reports the controllers pose of the controller's tip, while WebXR
// needs to report the pose of the controller's grip (centered on the user's
// palm.) This experimentally determined value is how far back along the Z axis
// in meters OpenVR's pose needs to be translated to align with WebXR's
// coordinate system.
const float kGripOffsetZMeters = 0.08f;

// WebXR reports a pointer pose separate from the grip pose, which represents a
// pointer ray emerging from the tip of the controller. OpenVR does not report
// anything like that, and most pointers are assumed to come straight from the
// controller's tip. For consistency with other WebXR backends we'll synthesize
// a pointer ray that's angled down slightly from the controller's handle,
// defined by this angle. Experimentally determined, should roughly point in the
// same direction as a user's outstretched index finger while holding a
// controller.
const float kPointerErgoAngleDegrees = -40.0f;

gfx::Transform HmdMatrix34ToTransform(const vr::HmdMatrix34_t& mat) {
  return gfx::Transform(mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3],
                        mat.m[1][0], mat.m[1][1], mat.m[1][2], mat.m[1][3],
                        mat.m[2][0], mat.m[2][1], mat.m[2][2], mat.m[2][3], 0,
                        0, 0, 1);
}

device::mojom::XRHandedness ConvertToMojoHandedness(
    vr::ETrackedControllerRole controller_role) {
  switch (controller_role) {
    case vr::TrackedControllerRole_LeftHand:
      return device::mojom::XRHandedness::LEFT;
    case vr::TrackedControllerRole_RightHand:
      return device::mojom::XRHandedness::RIGHT;
    case vr::TrackedControllerRole_Invalid:
      return device::mojom::XRHandedness::NONE;
  }

  NOTREACHED();
}

}  // namespace

void OpenVRRenderLoop::InputActiveState::MarkAsInactive() {
  active = false;
  primary_input_pressed = false;
  device_class = vr::TrackedDeviceClass_Invalid;
  controller_role = vr::TrackedControllerRole_Invalid;
}

OpenVRRenderLoop::OpenVRRenderLoop() : XRCompositorCommon() {}

OpenVRRenderLoop::~OpenVRRenderLoop() {
  Stop();
}

bool OpenVRRenderLoop::PreComposite() {
  texture_helper_.AllocateBackBuffer();
  return true;
}

bool OpenVRRenderLoop::SubmitCompositedFrame() {
  DCHECK(openvr_);
  vr::IVRCompositor* vr_compositor = openvr_->GetCompositor();
  DCHECK(vr_compositor);
  if (!vr_compositor)
    return false;

  vr::Texture_t texture;
  texture.handle = texture_helper_.GetBackbuffer().Get();
  texture.eType = vr::TextureType_DirectX;
  texture.eColorSpace = vr::ColorSpace_Auto;

  gfx::RectF left_bounds = texture_helper_.BackBufferLeft();
  gfx::RectF right_bounds = texture_helper_.BackBufferRight();

  vr::VRTextureBounds_t bounds[2];
  bounds[0] = {left_bounds.x(), left_bounds.y(),
               left_bounds.width() + left_bounds.x(),
               left_bounds.height() + left_bounds.y()};
  bounds[1] = {right_bounds.x(), right_bounds.y(),
               right_bounds.width() + right_bounds.x(),
               right_bounds.height() + right_bounds.y()};

  vr::EVRCompositorError error =
      vr_compositor->Submit(vr::EVREye::Eye_Left, &texture, &bounds[0]);
  if (error != vr::VRCompositorError_None) {
    return false;
  }
  error = vr_compositor->Submit(vr::EVREye::Eye_Right, &texture, &bounds[1]);
  if (error != vr::VRCompositorError_None) {
    return false;
  }
  vr_compositor->PostPresentHandoff();
  return true;
}

bool OpenVRRenderLoop::StartRuntime() {
  if (!openvr_) {
    openvr_ = std::make_unique<OpenVRWrapper>(true);
    if (!openvr_->IsInitialized()) {
      openvr_ = nullptr;
      return false;
    }

    openvr_->GetCompositor()->SuspendRendering(true);
    openvr_->GetCompositor()->SetTrackingSpace(
        vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
  }

#if defined(OS_WIN)
  int32_t adapter_index;
  openvr_->GetSystem()->GetDXGIOutputInfo(&adapter_index);
  if (!texture_helper_.SetAdapterIndex(adapter_index) ||
      !texture_helper_.EnsureInitialized()) {
    openvr_ = nullptr;
    return false;
  }
#endif

  uint32_t width, height;
  openvr_->GetSystem()->GetRecommendedRenderTargetSize(&width, &height);
  texture_helper_.SetDefaultSize(gfx::Size(width, height));

  return true;
}

void OpenVRRenderLoop::StopRuntime() {
  if (openvr_)
    openvr_->GetCompositor()->SuspendRendering(true);
  openvr_ = nullptr;
}

void OpenVRRenderLoop::OnSessionStart() {
  // Reset the active states for all the controllers.
  for (uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i) {
    InputActiveState& input_active_state = input_active_states_[i];
    input_active_state.active = false;
    input_active_state.primary_input_pressed = false;
    input_active_state.device_class = vr::TrackedDeviceClass_Invalid;
    input_active_state.controller_role = vr::TrackedControllerRole_Invalid;
  }

  openvr_->GetCompositor()->SuspendRendering(false);

  // Measure the VrViewerType we are presenting with.
  std::string model =
      GetOpenVRString(openvr_->GetSystem(), vr::Prop_ModelNumber_String);
  VrViewerType type = VrViewerType::OPENVR_UNKNOWN;
  if (model == "Oculus Rift CV1")
    type = VrViewerType::OPENVR_RIFT_CV1;
  else if (model == "Vive MV")
    type = VrViewerType::OPENVR_VIVE;

  LogViewerType(type);
}

mojom::VRPosePtr OpenVRRenderLoop::GetPose() {
  vr::TrackedDevicePose_t rendering_poses[vr::k_unMaxTrackedDeviceCount];

  TRACE_EVENT0("gpu", "WaitGetPoses");
  openvr_->GetCompositor()->WaitGetPoses(
      rendering_poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

  mojom::VRPosePtr pose = mojo::ConvertTo<mojom::VRPosePtr>(
      rendering_poses[vr::k_unTrackedDeviceIndex_Hmd]);

  // Update WebXR input sources.
  DCHECK(pose);
  pose->input_state =
      GetInputState(rendering_poses, vr::k_unMaxTrackedDeviceCount);

  return pose;
}

mojom::XRFrameDataPtr OpenVRRenderLoop::GetNextFrameData() {
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
  frame_data->frame_id = next_frame_id_;

  if (openvr_) {
    frame_data->pose = GetPose();
    vr::Compositor_FrameTiming timing;
    timing.m_nSize = sizeof(vr::Compositor_FrameTiming);
    bool valid_time = openvr_->GetCompositor()->GetFrameTiming(&timing);
    if (valid_time) {
      frame_data->time_delta =
          base::TimeDelta::FromSecondsD(timing.m_flSystemTimeInSeconds);
    }
  }

  return frame_data;
}

std::vector<mojom::XRInputSourceStatePtr> OpenVRRenderLoop::GetInputState(
    vr::TrackedDevicePose_t* poses,
    uint32_t count) {
  std::vector<mojom::XRInputSourceStatePtr> input_states;

  if (!openvr_)
    return input_states;

  // Loop through every device pose and determine which are controllers
  for (uint32_t i = vr::k_unTrackedDeviceIndex_Hmd + 1; i < count; ++i) {
    const vr::TrackedDevicePose_t& pose = poses[i];
    InputActiveState& input_active_state = input_active_states_[i];

    if (!pose.bDeviceIsConnected) {
      // If this was an active controller on the last frame report it as
      // disconnected.
      if (input_active_state.active)
        input_active_state.MarkAsInactive();
      continue;
    }

    // Is this a newly connected controller?
    bool newly_active = false;
    if (!input_active_state.active) {
      input_active_state.active = true;
      input_active_state.device_class =
          openvr_->GetSystem()->GetTrackedDeviceClass(i);
      newly_active = true;
    }

    // Skip over any tracked devices that aren't controllers.
    if (input_active_state.device_class != vr::TrackedDeviceClass_Controller) {
      continue;
    }

    device::mojom::XRInputSourceStatePtr state =
        device::mojom::XRInputSourceState::New();

    vr::VRControllerState_t controller_state;
    bool have_state = openvr_->GetSystem()->GetControllerState(
        i, &controller_state, sizeof(vr::VRControllerState_t));
    if (!have_state) {
      input_active_state.MarkAsInactive();
      continue;
    }

    bool pressed = controller_state.ulButtonPressed &
                   vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);

    state->source_id = i;
    state->primary_input_pressed = pressed;
    state->primary_input_clicked =
        (!pressed && input_active_state.primary_input_pressed);

    input_active_state.primary_input_pressed = pressed;

    if (pose.bPoseIsValid) {
      state->mojo_from_input =
          HmdMatrix34ToTransform(pose.mDeviceToAbsoluteTracking);
      // Scoot the grip matrix back a bit so that it actually lines up with the
      // user's palm.
      state->mojo_from_input->Translate3d(0, 0, kGripOffsetZMeters);
    }

    // Poll controller roll per-frame, since OpenVR controllers can swap hands.
    vr::ETrackedControllerRole controller_role =
        openvr_->GetSystem()->GetControllerRoleForTrackedDeviceIndex(i);

    device::mojom::XRHandedness handedness =
        ConvertToMojoHandedness(controller_role);

    OpenVRInputSourceData input_source_data =
        OpenVRGamepadHelper::GetXRInputSourceData(openvr_->GetSystem(), i,
                                                  controller_state, handedness);
    state->gamepad = input_source_data.gamepad;

    // OpenVR controller are fully 6DoF.
    state->emulated_position = false;

    // Re-send the controller's description if it's newly active or if the
    // handedness or profile strings have changed.
    if (newly_active ||
        (controller_role != input_active_state.controller_role) ||
        (input_source_data.profiles != input_active_state.profiles)) {
      device::mojom::XRInputSourceDescriptionPtr desc =
          device::mojom::XRInputSourceDescription::New();

      // It's a handheld pointing device.
      desc->target_ray_mode = device::mojom::XRTargetRayMode::POINTING;

      desc->handedness = handedness;
      input_active_state.controller_role = controller_role;

      // Tweak the pointer transform so that it's angled down from the
      // grip. This should be a bit more ergonomic.
      desc->input_from_pointer = gfx::Transform();
      desc->input_from_pointer->RotateAboutXAxis(kPointerErgoAngleDegrees);

      desc->profiles = input_source_data.profiles;

      state->description = std::move(desc);

      // Keep track of the current profiles so we know if it changes next frame.
      input_active_state.profiles = input_source_data.profiles;
    }

    input_states.push_back(std::move(state));
  }

  return input_states;
}

OpenVRRenderLoop::InputActiveState::InputActiveState() = default;
OpenVRRenderLoop::InputActiveState::~InputActiveState() = default;

}  // namespace device
