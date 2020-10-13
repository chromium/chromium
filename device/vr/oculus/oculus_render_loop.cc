// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/oculus/oculus_render_loop.h"

#include "device/vr/oculus/oculus_gamepad_helper.h"
#include "device/vr/oculus/oculus_type_converters.h"
#include "third_party/libovr/src/Include/Extras/OVR_Math.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"
#include "third_party/libovr/src/Include/OVR_CAPI_D3D.h"
#include "ui/gfx/geometry/angle_conversions.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace device {

namespace {

// How far the index trigger needs to be pushed to be considered "pressed".
const float kTriggerPressedThreshold = 0.6f;

// WebXR reports a pointer pose separate from the grip pose, which represents a
// pointer ray emerging from the tip of the controller. Oculus does not report
// anything like that, but the pose they report matches WebXR's idea of the
// pointer pose more than grip. For consistency with other WebXR backends we
// apply a rotation and slight translation to the reported pose to get the grip
// pose. Experimentally determined, should roughly place the grip pose origin at
// the center of the Oculus Touch handle.
const float kGripRotationXDelta = 45.0f;
const float kGripOffsetZMeters = 0.025f;

gfx::Transform PoseToTransform(const ovrPosef& pose) {
  OVR::Matrix4f mat(pose);
  return gfx::Transform(mat.M[0][0], mat.M[0][1], mat.M[0][2], mat.M[0][3],
                        mat.M[1][0], mat.M[1][1], mat.M[1][2], mat.M[1][3],
                        mat.M[2][0], mat.M[2][1], mat.M[2][2], mat.M[2][3],
                        mat.M[3][0], mat.M[3][1], mat.M[3][2], mat.M[3][3]);
}

}  // namespace

OculusRenderLoop::OculusRenderLoop() : XRCompositorCommon() {}

OculusRenderLoop::~OculusRenderLoop() {
  Stop();
}

mojom::XRFrameDataPtr OculusRenderLoop::GetNextFrameData() {
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
  frame_data->frame_id = next_frame_id_;

  if (!session_) {
    return frame_data;
  }

  auto predicted_time =
      ovr_GetPredictedDisplayTime(session_, ovr_frame_index_ + 1);
  ovrTrackingState state = ovr_GetTrackingState(session_, predicted_time, true);
  sensor_time_ = ovr_GetTimeInSeconds();
  frame_data->time_delta = base::TimeDelta::FromSecondsD(predicted_time);

  frame_data->pose = mojo::ConvertTo<mojom::VRPosePtr>(state.HeadPose.ThePose);
  last_render_pose_ = state.HeadPose.ThePose;

  frame_data->input_state = GetInputState(state);
  return frame_data;
}

bool OculusRenderLoop::StartRuntime() {
  if (!session_) {
    ovrInitParams initParams = {ovrInit_RequestVersion | ovrInit_MixedRendering,
                                OVR_MINOR_VERSION, NULL, 0, 0};
    ovrResult result = ovr_Initialize(&initParams);
    if (OVR_FAILURE(result)) {
      return false;
    }

    result = ovr_Create(&session_, &luid_);
    if (OVR_FAILURE(result)) {
      ovr_Shutdown();
      return false;
    }
  }

  if (!session_
#if defined(OS_WIN)
      || !texture_helper_.SetAdapterLUID(*reinterpret_cast<LUID*>(&luid_)) ||
      !texture_helper_.EnsureInitialized()
#endif
  ) {
    StopRuntime();
    return false;
  }

  ovrHmdDesc hmd_desc = ovr_GetHmdDesc(session_);
  auto render_desc = ovr_GetRenderDesc(session_, ovrEye_Left,
                                       hmd_desc.DefaultEyeFov[ovrEye_Left]);
  auto texture_size =
      ovr_GetFovTextureSize(session_, ovrEye_Left, render_desc.Fov, 1.0f);
  texture_helper_.SetDefaultSize(gfx::Size(texture_size.w, texture_size.h));

  return true;
}

void OculusRenderLoop::StopRuntime() {
  DestroyOvrSwapChain();
  if (session_) {
    // Shut down our current session so the presentation session can begin.
    ovr_Destroy(session_);
    session_ = nullptr;
    ovr_Shutdown();
  }

  texture_helper_.Reset();
  texture_swap_chain_ = 0;
  ovr_frame_index_ = 0;
}

void OculusRenderLoop::OnSessionStart() {
  LogViewerType(VrViewerType::OCULUS_UNKNOWN);
}

bool OculusRenderLoop::PreComposite() {
  // If our current swap chain has a different size than the recommended size
  // from texture helper (ie - either WebXR or Overlay size if only one is
  // visible, or the Rift's default size if both are visible), then we destroy
  // it and create a new one of the correct size.
  if (swap_chain_size_ != texture_helper_.BackBufferSize()) {
    DestroyOvrSwapChain();
  }

  // Create swap chain on demand.
  if (!texture_swap_chain_) {
    CreateOvrSwapChain();
  }

  if (!texture_swap_chain_)
    return false;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  ovr_GetTextureSwapChainBufferDX(
      session_, texture_swap_chain_, -1,
      IID_PPV_ARGS(texture.ReleaseAndGetAddressOf()));
  texture_helper_.SetBackbuffer(texture);

  return true;
}

bool OculusRenderLoop::SubmitCompositedFrame() {
  ovrLayerEyeFov layer = {};
  layer.Header.Type = ovrLayerType_EyeFov;
  layer.Header.Flags = 0;
  layer.ColorTexture[0] = texture_swap_chain_;

  gfx::RectF left_bounds = texture_helper_.BackBufferLeft();
  gfx::RectF right_bounds = texture_helper_.BackBufferRight();
  gfx::Size size = texture_helper_.BackBufferSize();

  DCHECK(size.width() % 2 == 0);
  layer.Viewport[0] = {
      // Left viewport.
      {static_cast<int>(size.width() * left_bounds.x()),
       static_cast<int>(size.height() * left_bounds.y())},
      {static_cast<int>(size.width() * left_bounds.width()),
       static_cast<int>(size.height() * left_bounds.height())}};

  layer.Viewport[1] = {
      // Right viewport.
      {static_cast<int>(size.width() * right_bounds.x()),
       static_cast<int>(size.height() * right_bounds.y())},
      {static_cast<int>(size.width() * right_bounds.width()),
       static_cast<int>(size.height() * right_bounds.height())}};
  ovrHmdDesc hmdDesc = ovr_GetHmdDesc(session_);
  layer.Fov[0] = hmdDesc.DefaultEyeFov[0];
  layer.Fov[1] = hmdDesc.DefaultEyeFov[1];

  auto render_desc_left = ovr_GetRenderDesc(session_, ovrEye_Left,
                                            hmdDesc.DefaultEyeFov[ovrEye_Left]);
  auto render_desc_right = ovr_GetRenderDesc(
      session_, ovrEye_Right, hmdDesc.DefaultEyeFov[ovrEye_Right]);
  ovrVector3f eye_offsets[2] = {render_desc_left.HmdToEyeOffset,
                                render_desc_right.HmdToEyeOffset};
  ovr_CalcEyePoses(last_render_pose_, eye_offsets, layer.RenderPose);

  layer.SensorSampleTime = sensor_time_;

  ovrViewScaleDesc view_scale_desc;
  view_scale_desc.HmdToEyeOffset[ovrEye_Left] = eye_offsets[ovrEye_Left];
  view_scale_desc.HmdToEyeOffset[ovrEye_Right] = eye_offsets[ovrEye_Right];
  view_scale_desc.HmdSpaceToWorldScaleInMeters = 1;

  constexpr unsigned int layer_count = 1;
  ovrLayerHeader* layer_headers[layer_count] = {&layer.Header};
  ovr_CommitTextureSwapChain(session_, texture_swap_chain_);
  ovrResult result = ovr_SubmitFrame(
      session_, ovr_frame_index_, &view_scale_desc, layer_headers, layer_count);
  if (!OVR_SUCCESS(result)) {
    // We failed to present.  Create a new swap chain.
    StopRuntime();
    StartRuntime();
    if (!session_) {
      return false;
    }
  }
  ovr_frame_index_++;

  return true;
}

void OculusRenderLoop::CreateOvrSwapChain() {
  ovrTextureSwapChainDesc desc = {};
  desc.Type = ovrTexture_2D;
  desc.ArraySize = 1;
  desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

  // Update our swap chain size to be the texture_helper_'s recommended size.
  // This will be the WebXR or Overlay size if only one is visible, or
  // the Rift's default framebuffer size if both are visible.
  swap_chain_size_ = texture_helper_.BackBufferSize();
  desc.Width = swap_chain_size_.width();
  desc.Height = swap_chain_size_.height();
  desc.MipLevels = 1;
  desc.SampleCount = 1;  // No anti-aliasing since we are just copying textures.
  desc.StaticImage = ovrFalse;
  desc.MiscFlags = ovrTextureMisc_DX_Typeless;
  desc.BindFlags = ovrTextureBind_DX_RenderTarget;

  // On failure, texture_swap_chain_ will be null and we'll handle that as an
  // error later.
  ovr_CreateTextureSwapChainDX(session_, texture_helper_.GetDevice().Get(),
                               &desc, &texture_swap_chain_);
}

void OculusRenderLoop::DestroyOvrSwapChain() {
  if (texture_swap_chain_) {
    ovr_DestroyTextureSwapChain(session_, texture_swap_chain_);
    texture_swap_chain_ = 0;
  }
}

void OculusRenderLoop::OnLayerBoundsChanged() {}

std::vector<mojom::XRInputSourceStatePtr> OculusRenderLoop::GetInputState(
    const ovrTrackingState& tracking_state) {
  std::vector<mojom::XRInputSourceStatePtr> input_states;

  ovrInputState input_state;

  // Get the state of the active controllers.
  if ((OVR_SUCCESS(ovr_GetInputState(session_, ovrControllerType_Active,
                                     &input_state)))) {
    // Report whichever touch controllers are currently active.
    if (input_state.ControllerType & ovrControllerType_LTouch) {
      input_states.push_back(GetTouchData(
          ovrControllerType_LTouch, tracking_state.HandPoses[ovrHand_Left],
          input_state, ovrHand_Left));
    } else {
      primary_input_pressed[ovrControllerType_LTouch] = false;
      primary_squeeze_pressed[ovrControllerType_LTouch] = false;
    }

    if (input_state.ControllerType & ovrControllerType_RTouch) {
      input_states.push_back(GetTouchData(
          ovrControllerType_RTouch, tracking_state.HandPoses[ovrHand_Right],
          input_state, ovrHand_Right));
    } else {
      primary_input_pressed[ovrControllerType_RTouch] = false;
      primary_squeeze_pressed[ovrControllerType_RTouch] = false;
    }

    // If an oculus remote is active, report a gaze controller.
    if (input_state.ControllerType & ovrControllerType_Remote) {
      device::mojom::XRInputSourceStatePtr state =
          device::mojom::XRInputSourceState::New();

      state->source_id = ovrControllerType_Remote;
      state->primary_input_pressed =
          (input_state.Buttons & ovrButton_Enter) != 0;
      state->primary_squeeze_pressed = false;
      state->primary_squeeze_clicked = false;

      if (!state->primary_input_pressed &&
          primary_input_pressed[ovrControllerType_Remote]) {
        state->primary_input_clicked = true;
      }

      // TODO(https://crbug.com/956190): Expose remote as a gamepad to WebXR.

      primary_input_pressed[ovrControllerType_Remote] =
          state->primary_input_pressed;

      input_states.push_back(std::move(state));
    } else {
      primary_input_pressed[ovrControllerType_Remote] = false;
      primary_squeeze_pressed[ovrControllerType_Remote] = false;
    }
  }

  return input_states;
}

device::mojom::XRInputSourceStatePtr OculusRenderLoop::GetTouchData(
    ovrControllerType type,
    const ovrPoseStatef& pose,
    const ovrInputState& input_state,
    ovrHandType hand) {
  device::mojom::XRInputSourceStatePtr state =
      device::mojom::XRInputSourceState::New();

  state->source_id = type;
  state->primary_input_pressed =
      (input_state.IndexTrigger[hand] > kTriggerPressedThreshold);

  // If the input has gone from pressed to not pressed since the last poll
  // report it as clicked.
  if (!state->primary_input_pressed && primary_input_pressed[type])
    state->primary_input_clicked = true;

  primary_input_pressed[type] = state->primary_input_pressed;

  // Handling squeeze
  state->primary_squeeze_pressed =
      (input_state.HandTrigger[hand] > kTriggerPressedThreshold);

  // If the input has gone from pressed to not pressed since the last poll
  // report it as clicked.
  if (!state->primary_squeeze_pressed && primary_squeeze_pressed[type])
    state->primary_squeeze_clicked = true;

  primary_squeeze_pressed[type] = state->primary_squeeze_pressed;

  device::mojom::XRInputSourceDescriptionPtr desc =
      device::mojom::XRInputSourceDescription::New();

  // It's a handheld pointing device.
  desc->target_ray_mode = device::mojom::XRTargetRayMode::POINTING;

  // Set handedness.
  switch (hand) {
    case ovrHand_Left:
      desc->handedness = device::mojom::XRHandedness::LEFT;
      break;
    case ovrHand_Right:
      desc->handedness = device::mojom::XRHandedness::RIGHT;
      break;
    default:
      desc->handedness = device::mojom::XRHandedness::NONE;
      break;
  }

  // Touch controllers are fully 6DoF.
  state->emulated_position = false;

  // The grip pose will be rotated and translated back a bit from the pointer
  // pose, which is what the Oculus API returns.
  state->mojo_from_input = PoseToTransform(pose.ThePose);
  state->mojo_from_input->RotateAboutXAxis(kGripRotationXDelta);
  state->mojo_from_input->Translate3d(0, 0, kGripOffsetZMeters);

  // Need to apply the inverse transform from above to put the pointer back in
  // the right orientation relative to the grip.
  desc->input_from_pointer = gfx::Transform();
  desc->input_from_pointer->Translate3d(0, 0, -kGripOffsetZMeters);
  desc->input_from_pointer->RotateAboutXAxis(-kGripRotationXDelta);

  // This function is only called when we're working with an Oculus touch.
  desc->profiles.push_back("oculus-touch");

  // The absence of "touchpad" in this string indicates that the slots in the
  // button and axes arrays are placeholders required by the xr-standard mapping
  // but not actually updated with any input.
  desc->profiles.push_back("generic-trigger-squeeze-thumbstick");

  state->description = std::move(desc);

  state->gamepad = OculusGamepadHelper::CreateGamepad(session_, hand);

  return state;
}

}  // namespace device
