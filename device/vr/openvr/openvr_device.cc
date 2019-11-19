// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_device.h"

#include <math.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/math_constants.h"
#include "build/build_config.h"
#include "device/vr/openvr/openvr_render_loop.h"
#include "device/vr/openvr/openvr_type_converters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace device {

void OpenVRDevice::RecordRuntimeAvailability() {
  XrRuntimeAvailable runtime = XrRuntimeAvailable::NONE;
  if (vr::VR_IsRuntimeInstalled())
    runtime = XrRuntimeAvailable::OPENVR;
  UMA_HISTOGRAM_ENUMERATION("XR.RuntimeAvailable", runtime);
}

namespace {

constexpr base::TimeDelta kPollingInterval =
    base::TimeDelta::FromSecondsD(0.25);

mojom::VRFieldOfViewPtr OpenVRFovToWebVRFov(vr::IVRSystem* vr_system,
                                            vr::Hmd_Eye eye) {
  auto out = mojom::VRFieldOfView::New();
  float up_tan, down_tan, left_tan, right_tan;
  vr_system->GetProjectionRaw(eye, &left_tan, &right_tan, &up_tan, &down_tan);

  // TODO(billorr): Plumb the expected projection matrix over mojo instead of
  // using angles. Up and down are intentionally swapped to account for
  // differences in expected projection matrix format for GVR and OpenVR.
  out->up_degrees = gfx::RadToDeg(atanf(down_tan));
  out->down_degrees = -gfx::RadToDeg(atanf(up_tan));
  out->left_degrees = -gfx::RadToDeg(atanf(left_tan));
  out->right_degrees = gfx::RadToDeg(atanf(right_tan));
  return out;
}

gfx::Transform HmdMatrix34ToTransform(const vr::HmdMatrix34_t& mat) {
  // Disable formatting so that the 4x4 matrix is more readable
  // clang-format off
  return gfx::Transform(
     mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3],
     mat.m[1][0], mat.m[1][1], mat.m[1][2], mat.m[1][3],
     mat.m[2][0], mat.m[2][1], mat.m[2][2], mat.m[2][3],
     0.0f,        0.0f,        0.0f,        1.0f);
  // clang-format on
}

// OpenVR uses A_to_B convention for naming transformation matrices, but we pass
// matrices through mojo using the B_from_A naming convention since that what
// blink uses.
gfx::Transform HeadFromEyeTransform(vr::IVRSystem* vr_system, vr::Hmd_Eye eye) {
  return HmdMatrix34ToTransform(vr_system->GetEyeToHeadTransform(eye));
}

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(vr::IVRSystem* vr_system,
                                            device::mojom::XRDeviceId id) {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
  display_info->id = id;

  display_info->left_eye = mojom::VREyeParameters::New();
  display_info->right_eye = mojom::VREyeParameters::New();
  mojom::VREyeParametersPtr& left_eye = display_info->left_eye;
  mojom::VREyeParametersPtr& right_eye = display_info->right_eye;

  left_eye->field_of_view = OpenVRFovToWebVRFov(vr_system, vr::Eye_Left);
  right_eye->field_of_view = OpenVRFovToWebVRFov(vr_system, vr::Eye_Right);

  left_eye->head_from_eye = HeadFromEyeTransform(vr_system, vr::Eye_Left);
  right_eye->head_from_eye = HeadFromEyeTransform(vr_system, vr::Eye_Right);

  uint32_t width, height;
  vr_system->GetRecommendedRenderTargetSize(&width, &height);
  left_eye->render_width = width;
  left_eye->render_height = height;
  right_eye->render_width = left_eye->render_width;
  right_eye->render_height = left_eye->render_height;

  display_info->stage_parameters = mojom::VRStageParameters::New();
  vr::HmdMatrix34_t mat =
      vr_system->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
  display_info->stage_parameters->standing_transform =
      HmdMatrix34ToTransform(mat);

  vr::IVRChaperone* chaperone = vr::VRChaperone();
  if (chaperone) {
    chaperone->GetPlayAreaSize(&display_info->stage_parameters->size_x,
                               &display_info->stage_parameters->size_z);
  } else {
    display_info->stage_parameters->size_x = 0.0f;
    display_info->stage_parameters->size_z = 0.0f;
  }

  return display_info;
}


}  // namespace

OpenVRDevice::OpenVRDevice()
    : VRDeviceBase(device::mojom::XRDeviceId::OPENVR_DEVICE_ID),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  render_loop_ = std::make_unique<OpenVRRenderLoop>();

  OnPollingEvents();
}

bool OpenVRDevice::IsHwAvailable() {
  return vr::VR_IsHmdPresent();
}

bool OpenVRDevice::IsApiAvailable() {
  return vr::VR_IsRuntimeInstalled();
}

mojo::PendingRemote<mojom::XRCompositorHost>
OpenVRDevice::BindCompositorHost() {
  return compositor_host_receiver_.BindNewPipeAndPassRemote();
}

OpenVRDevice::~OpenVRDevice() {
  Shutdown();
}

void OpenVRDevice::Shutdown() {
  // Wait for the render loop to stop before completing destruction. This will
  // ensure that the IVRSystem doesn't get shutdown until the render loop is no
  // longer referencing it.
  if (render_loop_ && render_loop_->IsRunning())
    render_loop_->Stop();
}

void OpenVRDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  if (!EnsureValidDisplayInfo()) {
    std::move(callback).Run(nullptr, mojo::NullRemote());
    return;
  }

  DCHECK(options->immersive);

  if (!render_loop_->IsRunning()) {
    render_loop_->Start();

    if (!render_loop_->IsRunning()) {
      std::move(callback).Run(nullptr, mojo::NullRemote());
      return;
    }

    if (overlay_receiver_) {
      render_loop_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                    base::Unretained(render_loop_.get()),
                                    std::move(overlay_receiver_)));
    }
  }

  // We are done using OpenVR until the presentation session ends.
  openvr_ = nullptr;

  auto my_callback =
      base::BindOnce(&OpenVRDevice::OnRequestSessionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  auto on_presentation_ended = base::BindOnce(
      &OpenVRDevice::OnPresentationEnded, weak_ptr_factory_.GetWeakPtr());

  render_loop_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&XRCompositorCommon::RequestSession,
                     base::Unretained(render_loop_.get()),
                     std::move(on_presentation_ended),
                     base::DoNothing::Repeatedly<mojom::XRVisibilityState>(),
                     std::move(options), std::move(my_callback)));
  outstanding_session_requests_count_++;
}

bool OpenVRDevice::EnsureValidDisplayInfo() {
  // Ensure we have had a valid display_info set at least once.
  if (!have_real_display_info_) {
    DCHECK(!openvr_);
    // Initialize OpenVR.
    openvr_ = std::make_unique<OpenVRWrapper>(false /* presenting */);
    if (!openvr_->IsInitialized()) {
      openvr_ = nullptr;
      return false;
    }

    SetVRDisplayInfo(CreateVRDisplayInfo(openvr_->GetSystem(), GetId()));
    have_real_display_info_ = true;
  }
  return have_real_display_info_;
}

void OpenVRDevice::OnPresentationEnded() {
  if (!openvr_ && outstanding_session_requests_count_ == 0) {
    openvr_ = std::make_unique<OpenVRWrapper>(false /* presenting */);
    if (!openvr_->IsInitialized()) {
      openvr_ = nullptr;
      return;
    }
  }
}

void OpenVRDevice::OnRequestSessionResult(
    mojom::XRRuntime::RequestSessionCallback callback,
    bool result,
    mojom::XRSessionPtr session) {
  outstanding_session_requests_count_--;
  if (!result) {
    OnPresentationEnded();
    std::move(callback).Run(nullptr, mojo::NullRemote());
    return;
  }

  OnStartPresenting();

  session->display_info = display_info_.Clone();

  std::move(callback).Run(
      std::move(session),
      exclusive_controller_receiver_.BindNewPipeAndPassRemote());

  // Use of Unretained is safe because the callback will only occur if the
  // binding is not destroyed.
  exclusive_controller_receiver_.set_disconnect_handler(
      base::BindOnce(&OpenVRDevice::OnPresentingControllerMojoConnectionError,
                     base::Unretained(this)));
}

bool OpenVRDevice::IsAvailable() {
  return vr::VR_IsRuntimeInstalled() && vr::VR_IsHmdPresent();
}

void OpenVRDevice::CreateImmersiveOverlay(
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

// XRSessionController
void OpenVRDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  DCHECK(false);
}

void OpenVRDevice::OnPresentingControllerMojoConnectionError() {
  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&XRCompositorCommon::ExitPresent,
                                base::Unretained(render_loop_.get())));
  OnExitPresent();
  exclusive_controller_receiver_.reset();
}

// Only deal with events that will cause displayInfo changes for now.
void OpenVRDevice::OnPollingEvents() {
  main_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OpenVRDevice::OnPollingEvents,
                     weak_ptr_factory_.GetWeakPtr()),
      kPollingInterval);

  if (!openvr_)
    return;

  vr::VREvent_t event;
  bool is_changed = false;
  while (openvr_->GetSystem()->PollNextEvent(&event, sizeof(event))) {
    if (event.trackedDeviceIndex != vr::k_unTrackedDeviceIndex_Hmd &&
        event.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid) {
      continue;
    }

    switch (event.eventType) {
      case vr::VREvent_TrackedDeviceUpdated:
      case vr::VREvent_IpdChanged:
      case vr::VREvent_ChaperoneDataHasChanged:
      case vr::VREvent_ChaperoneSettingsHaveChanged:
      case vr::VREvent_ChaperoneUniverseHasChanged:
        is_changed = true;
        break;

      default:
        break;
    }
  }

  if (is_changed)
    SetVRDisplayInfo(CreateVRDisplayInfo(openvr_->GetSystem(), GetId()));
}

}  // namespace device
