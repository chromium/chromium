// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_device.h"

#include <math.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/math_constants.h"
#include "build/build_config.h"
#include "device/vr/openvr/openvr_render_loop.h"
#include "device/vr/openvr/openvr_type_converters.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace device {

namespace {

constexpr float kDefaultIPD = 0.06f;  // Default average IPD.
constexpr double kTimeBetweenPollingEventsSeconds = 0.25;

mojom::VRFieldOfViewPtr OpenVRFovToWebVRFov(vr::IVRSystem* vr_system,
                                            vr::Hmd_Eye eye) {
  auto out = mojom::VRFieldOfView::New();
  float up_tan, down_tan, left_tan, right_tan;
  vr_system->GetProjectionRaw(eye, &left_tan, &right_tan, &up_tan, &down_tan);

  // TODO(billorr): Plumb the expected projection matrix over mojo instead of
  // using angles. Up and down are intentionally swapped to account for
  // differences in expected projection matrix format for GVR and OpenVR.
  out->upDegrees = gfx::RadToDeg(atanf(down_tan));
  out->downDegrees = -gfx::RadToDeg(atanf(up_tan));
  out->leftDegrees = -gfx::RadToDeg(atanf(left_tan));
  out->rightDegrees = gfx::RadToDeg(atanf(right_tan));
  return out;
}

std::vector<float> HmdMatrix34ToWebVRTransformMatrix(
    const vr::HmdMatrix34_t& mat) {
  std::vector<float> transform;
  transform.resize(16);
  transform[0] = mat.m[0][0];
  transform[1] = mat.m[1][0];
  transform[2] = mat.m[2][0];
  transform[3] = 0.0f;
  transform[4] = mat.m[0][1];
  transform[5] = mat.m[1][1];
  transform[6] = mat.m[2][1];
  transform[7] = 0.0f;
  transform[8] = mat.m[0][2];
  transform[9] = mat.m[1][2];
  transform[10] = mat.m[2][2];
  transform[11] = 0.0f;
  transform[12] = mat.m[0][3];
  transform[13] = mat.m[1][3];
  transform[14] = mat.m[2][3];
  transform[15] = 1.0f;
  return transform;
}

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(vr::IVRSystem* vr_system,
                                            device::mojom::XRDeviceId id) {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
  display_info->id = id;
  display_info->displayName =
      GetOpenVRString(vr_system, vr::Prop_ManufacturerName_String) + " " +
      GetOpenVRString(vr_system, vr::Prop_ModelNumber_String);
  display_info->capabilities = mojom::VRDisplayCapabilities::New();
  display_info->capabilities->hasPosition = true;
  display_info->capabilities->hasExternalDisplay = true;
  display_info->capabilities->canPresent = true;
  display_info->webvr_default_framebuffer_scale = 1.0;
  display_info->webxr_default_framebuffer_scale = 1.0;

  display_info->leftEye = mojom::VREyeParameters::New();
  display_info->rightEye = mojom::VREyeParameters::New();
  mojom::VREyeParametersPtr& left_eye = display_info->leftEye;
  mojom::VREyeParametersPtr& right_eye = display_info->rightEye;

  left_eye->fieldOfView = OpenVRFovToWebVRFov(vr_system, vr::Eye_Left);
  right_eye->fieldOfView = OpenVRFovToWebVRFov(vr_system, vr::Eye_Right);

  vr::TrackedPropertyError error = vr::TrackedProp_Success;
  float ipd = vr_system->GetFloatTrackedDeviceProperty(
      vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserIpdMeters_Float, &error);

  if (error != vr::TrackedProp_Success)
    ipd = kDefaultIPD;

  left_eye->offset.resize(3);
  left_eye->offset[0] = -ipd * 0.5;
  left_eye->offset[1] = 0.0f;
  left_eye->offset[2] = 0.0f;
  right_eye->offset.resize(3);
  right_eye->offset[0] = ipd * 0.5;
  right_eye->offset[1] = 0.0;
  right_eye->offset[2] = 0.0;

  uint32_t width, height;
  vr_system->GetRecommendedRenderTargetSize(&width, &height);
  left_eye->renderWidth = width;
  left_eye->renderHeight = height;
  right_eye->renderWidth = left_eye->renderWidth;
  right_eye->renderHeight = left_eye->renderHeight;

  display_info->stageParameters = mojom::VRStageParameters::New();
  vr::HmdMatrix34_t mat =
      vr_system->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
  display_info->stageParameters->standingTransform =
      HmdMatrix34ToWebVRTransformMatrix(mat);

  vr::IVRChaperone* chaperone = vr::VRChaperone();
  if (chaperone) {
    chaperone->GetPlayAreaSize(&display_info->stageParameters->sizeX,
                               &display_info->stageParameters->sizeZ);
  } else {
    display_info->stageParameters->sizeX = 0.0f;
    display_info->stageParameters->sizeZ = 0.0f;
  }

  return display_info;
}


}  // namespace

OpenVRDevice::OpenVRDevice()
    : VRDeviceBase(device::mojom::XRDeviceId::OPENVR_DEVICE_ID),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      exclusive_controller_binding_(this),
      gamepad_provider_factory_binding_(this),
      compositor_host_binding_(this),
      weak_ptr_factory_(this) {
  // Initialize OpenVR.
  openvr_ = std::make_unique<OpenVRWrapper>(false /* presenting */);
  if (!openvr_->IsInitialized()) {
    openvr_ = nullptr;
    return;
  }

  SetVRDisplayInfo(CreateVRDisplayInfo(openvr_->GetSystem(), GetId()));

  render_loop_ = std::make_unique<OpenVRRenderLoop>();

  OnPollingEvents();
}

mojom::IsolatedXRGamepadProviderFactoryPtr OpenVRDevice::BindGamepadFactory() {
  mojom::IsolatedXRGamepadProviderFactoryPtr ret;
  gamepad_provider_factory_binding_.Bind(mojo::MakeRequest(&ret));
  return ret;
}

mojom::XRCompositorHostPtr OpenVRDevice::BindCompositorHost() {
  mojom::XRCompositorHostPtr ret;
  compositor_host_binding_.Bind(mojo::MakeRequest(&ret));
  return ret;
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
  if (!options->immersive) {
    ReturnNonImmersiveSession(std::move(callback));
    return;
  }

  if (!render_loop_->IsRunning()) {
    render_loop_->Start();

    if (!render_loop_->IsRunning()) {
      std::move(callback).Run(nullptr, nullptr);
      return;
    }

    if (provider_request_) {
      render_loop_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestGamepadProvider,
                                    base::Unretained(render_loop_.get()),
                                    std::move(provider_request_)));
    }

    if (overlay_request_) {
      render_loop_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                    base::Unretained(render_loop_.get()),
                                    std::move(overlay_request_)));
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
      FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestSession,
                                base::Unretained(render_loop_.get()),
                                std::move(on_presentation_ended),
                                std::move(options), std::move(my_callback)));
}

void OpenVRDevice::OnPresentationEnded() {
  if (!openvr_) {
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
  if (!result) {
    OnPresentationEnded();
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  OnStartPresenting();

  mojom::XRSessionControllerPtr session_controller;
  exclusive_controller_binding_.Bind(mojo::MakeRequest(&session_controller));

  // Use of Unretained is safe because the callback will only occur if the
  // binding is not destroyed.
  exclusive_controller_binding_.set_connection_error_handler(
      base::BindOnce(&OpenVRDevice::OnPresentingControllerMojoConnectionError,
                     base::Unretained(this)));

  session->display_info = display_info_.Clone();

  std::move(callback).Run(std::move(session), std::move(session_controller));
}

void OpenVRDevice::GetIsolatedXRGamepadProvider(
    mojom::IsolatedXRGamepadProviderRequest provider_request) {
  if (render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestGamepadProvider,
                                  base::Unretained(render_loop_.get()),
                                  std::move(provider_request)));
  } else {
    provider_request_ = std::move(provider_request);
  }
}

void OpenVRDevice::CreateImmersiveOverlay(
    mojom::ImmersiveOverlayRequest overlay_request) {
  if (render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                  base::Unretained(render_loop_.get()),
                                  std::move(overlay_request)));
  } else {
    overlay_request_ = std::move(overlay_request);
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
  // Don't stop the render loop here. We need to keep the gamepad provider alive
  // so that we don't lose a pending mojo gamepad_callback_.
  // TODO(https://crbug.com/875187): Alternatively, we could recreate the
  // provider on the next session, or look into why the callback gets lost.
  OnExitPresent();
  exclusive_controller_binding_.Close();
}

void OpenVRDevice::OnMagicWindowFrameDataRequest(
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  if (!openvr_) {
    std::move(callback).Run(nullptr);
    return;
  }
  const float kPredictionTimeSeconds = 0.03f;
  vr::TrackedDevicePose_t rendering_poses[vr::k_unMaxTrackedDeviceCount];
  openvr_->GetSystem()->GetDeviceToAbsoluteTrackingPose(
      vr::TrackingUniverseSeated, kPredictionTimeSeconds, rendering_poses,
      vr::k_unMaxTrackedDeviceCount);
  mojom::XRFrameDataPtr data = mojom::XRFrameData::New();
  data->pose = mojo::ConvertTo<mojom::VRPosePtr>(
      rendering_poses[vr::k_unTrackedDeviceIndex_Hmd]);
  std::move(callback).Run(std::move(data));
}

// Only deal with events that will cause displayInfo changes for now.
void OpenVRDevice::OnPollingEvents() {
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

  main_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OpenVRDevice::OnPollingEvents,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSecondsD(kTimeBetweenPollingEventsSeconds));
}

}  // namespace device
