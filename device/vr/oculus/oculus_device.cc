// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/oculus/oculus_device.h"

#include <math.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/math_constants.h"
#include "build/build_config.h"
#include "device/vr/oculus/oculus_render_loop.h"
#include "device/vr/oculus/oculus_type_converters.h"
#include "device/vr/util/stage_utils.h"
#include "device/vr/util/transform_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"
#include "third_party/libovr/src/Include/OVR_CAPI_D3D.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace device {

namespace {

mojom::VREyeParametersPtr GetEyeDetails(ovrSession session,
                                        const ovrHmdDesc& hmd_desc,
                                        ovrEyeType eye) {
  auto eye_parameters = mojom::VREyeParameters::New();
  auto render_desc =
      ovr_GetRenderDesc(session, eye, hmd_desc.DefaultEyeFov[eye]);
  eye_parameters->field_of_view = mojom::VRFieldOfView::New();
  eye_parameters->field_of_view->up_degrees =
      gfx::RadToDeg(atanf(render_desc.Fov.UpTan));
  eye_parameters->field_of_view->down_degrees =
      gfx::RadToDeg(atanf(render_desc.Fov.DownTan));
  eye_parameters->field_of_view->left_degrees =
      gfx::RadToDeg(atanf(render_desc.Fov.LeftTan));
  eye_parameters->field_of_view->right_degrees =
      gfx::RadToDeg(atanf(render_desc.Fov.RightTan));

  // TODO(crbug.com/999353): Query eye-to-head transform from the device and use
  // that instead of just building a transformation matrix from the translation
  // component. This requireds updating libovr to v1.25 because v1.16 doesn't
  // have HmdToEyePose (tracked by crbug.com/999355).
  auto offset = render_desc.HmdToEyeOffset;
  eye_parameters->head_from_eye =
      vr_utils::MakeTranslationTransform(offset.x, offset.y, offset.z);

  auto texture_size =
      ovr_GetFovTextureSize(session, eye, render_desc.Fov, 1.0f);
  eye_parameters->render_width = texture_size.w;
  eye_parameters->render_height = texture_size.h;

  return eye_parameters;
}

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(ovrSession session) {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();

  ovrHmdDesc hmdDesc = ovr_GetHmdDesc(session);
  display_info->left_eye = GetEyeDetails(session, hmdDesc, ovrEye_Left);
  display_info->right_eye = GetEyeDetails(session, hmdDesc, ovrEye_Right);

  display_info->stage_parameters = mojom::VRStageParameters::New();
  ovr_SetTrackingOriginType(session, ovrTrackingOrigin_FloorLevel);
  ovrTrackingState ovr_state = ovr_GetTrackingState(session, 0, true);
  float floor_height = ovr_state.HeadPose.ThePose.Position.y;
  ovr_SetTrackingOriginType(session, ovrTrackingOrigin_EyeLevel);

  gfx::Transform mojo_from_floor;
  mojo_from_floor.Translate3d(0, -1 * floor_height, 0);
  display_info->stage_parameters->mojo_from_floor = mojo_from_floor;

  ovrVector3f boundary_size;
  ovr_GetBoundaryDimensions(session, ovrBoundary_PlayArea, &boundary_size);
  display_info->stage_parameters->bounds =
      vr_utils::GetStageBoundsFromSize(boundary_size.x, boundary_size.z);

  return display_info;
}

}  // namespace

OculusDevice::OculusDevice()
    : VRDeviceBase(mojom::XRDeviceId::OCULUS_DEVICE_ID),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  render_loop_ = std::make_unique<OculusRenderLoop>();
}

bool OculusDevice::IsHwAvailable() {
  auto result = ovr_Detect(0);
  return result.IsOculusHMDConnected;
}

bool OculusDevice::IsApiAvailable() {
  auto result = ovr_Detect(0);
  return result.IsOculusServiceRunning;
}

mojo::PendingRemote<mojom::XRCompositorHost>
OculusDevice::BindCompositorHost() {
  return compositor_host_receiver_.BindNewPipeAndPassRemote();
}

OculusDevice::~OculusDevice() {
  // Wait for the render loop to stop before completing destruction. This will
  // ensure that bindings are closed on the correct thread.
  if (render_loop_ && render_loop_->IsRunning())
    render_loop_->Stop();

  StopOvrSession();
}

void OculusDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  if (!EnsureValidDisplayInfo()) {
    std::move(callback).Run(nullptr, mojo::NullRemote());
    return;
  }

  DCHECK_EQ(options->mode, mojom::XRSessionMode::kImmersiveVr);

  StopOvrSession();

  if (!render_loop_->IsRunning()) {
    render_loop_->Start();

    if (!render_loop_->IsRunning()) {
      std::move(callback).Run(nullptr, mojo::NullRemote());
      StartOvrSession();
      return;
    }

    if (overlay_receiver_) {
      render_loop_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                    base::Unretained(render_loop_.get()),
                                    std::move(overlay_receiver_)));
    }
  }

  auto on_request_present_result =
      base::BindOnce(&OculusDevice::OnRequestSessionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  auto on_presentation_ended = base::BindOnce(
      &OculusDevice::OnPresentationEnded, weak_ptr_factory_.GetWeakPtr());

  render_loop_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&XRCompositorCommon::RequestSession,
                     base::Unretained(render_loop_.get()),
                     std::move(on_presentation_ended),
                     base::DoNothing::Repeatedly<mojom::XRVisibilityState>(),
                     std::move(options), std::move(on_request_present_result)));
  outstanding_session_requests_count_++;
}

bool OculusDevice::EnsureValidDisplayInfo() {
  // Ensure we have had a valid display_info set at least once.
  if (!have_real_display_info_) {
    // Initialize Oculus briefly.
    StartOvrSession();
    if (!session_) {
      return false;
    }

    SetVRDisplayInfo(CreateVRDisplayInfo(session_));
    have_real_display_info_ = true;
  }
  return have_real_display_info_;
}

void OculusDevice::OnRequestSessionResult(
    mojom::XRRuntime::RequestSessionCallback callback,
    bool result,
    mojom::XRSessionPtr session) {
  outstanding_session_requests_count_--;
  if (!result) {
    std::move(callback).Run(nullptr, mojo::NullRemote());

    // Start magic window again.
    if (outstanding_session_requests_count_ == 0)
      StartOvrSession();
    return;
  }

  OnStartPresenting();

  session->display_info = display_info_.Clone();

  std::move(callback).Run(
      std::move(session),
      exclusive_controller_receiver_.BindNewPipeAndPassRemote());

  // Unretained is safe because the error handler won't be called after the
  // binding has been destroyed.
  exclusive_controller_receiver_.set_disconnect_handler(
      base::BindOnce(&OculusDevice::OnPresentingControllerMojoConnectionError,
                     base::Unretained(this)));
}

bool OculusDevice::IsAvailable() {
  auto result = ovr_Detect(0);
  return result.IsOculusHMDConnected && result.IsOculusServiceRunning;
}

// XRSessionController
void OculusDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  DCHECK(false);
}

void OculusDevice::OnPresentingControllerMojoConnectionError() {
  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&XRCompositorCommon::ExitPresent,
                                base::Unretained(render_loop_.get())));
  OnExitPresent();
  exclusive_controller_receiver_.reset();
}

void OculusDevice::OnPresentationEnded() {
  // If we are no-longer presenting, and there are no outstanding requests to
  // start presenting, start the Oculus API to allow magic-window.
  if (outstanding_session_requests_count_ == 0)
    StartOvrSession();
}

void OculusDevice::StartOvrSession() {
  DCHECK_EQ(outstanding_session_requests_count_, 0);
  ovrInitParams initParams = {ovrInit_RequestVersion | ovrInit_Invisible,
                              OVR_MINOR_VERSION, NULL, 0, 0};
  ovrResult result = ovr_Initialize(&initParams);
  if (OVR_FAILURE(result)) {
    return;
  }

  ovrGraphicsLuid luid;
  result = ovr_Create(&session_, &luid);
  if (OVR_FAILURE(result)) {
    return;
  }
}

void OculusDevice::StopOvrSession() {
  if (session_) {
    // Shut down our current session so the presentation session can begin.
    ovr_Destroy(session_);
    session_ = nullptr;
    ovr_Shutdown();
  }
}

void OculusDevice::CreateImmersiveOverlay(
    mojo::PendingReceiver<mojom::ImmersiveOverlay> overlay_receiver) {
  if (render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                  base::Unretained(render_loop_.get()),
                                  std::move(overlay_receiver)));
  } else {
    overlay_receiver_ = std::move(overlay_receiver);
  }
}

}  // namespace device
