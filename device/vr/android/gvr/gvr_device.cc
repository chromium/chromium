// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device.h"

#include <math.h>
#include <algorithm>
#include <utility>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/vr_display_impl.h"
#include "jni/NonPresentingGvrContext_jni.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

using base::android::JavaRef;

namespace device {

namespace {

// Default downscale factor for computing the recommended WebVR/WebXR
// renderWidth/Height from the 1:1 pixel mapped size. Using a rather
// aggressive downscale due to the high overhead of copying pixels
// twice before handing off to GVR. For comparison, the polyfill
// uses approximately 0.55 on a Pixel XL.
static constexpr float kWebVrRecommendedResolutionScale = 0.5;
static constexpr float kWebXrRecommendedResolutionScale = 0.7;

// The scale factor for WebXR on devices that don't have shared buffer
// support. (Android N and earlier.)
static constexpr float kWebXrNoSharedBufferResolutionScale = 0.5;

gfx::Size GetMaximumWebVrSize(gvr::GvrApi* gvr_api) {
  // Get the default, unscaled size for the WebVR transfer surface
  // based on the optimal 1:1 render resolution. A scalar will be applied to
  // this value in the renderer to reduce the render load. This size will also
  // be reported to the client via CreateVRDisplayInfo as the
  // client-recommended renderWidth/renderHeight and for the GVR
  // framebuffer. If the client chooses a different size or resizes it
  // while presenting, we'll resize the transfer surface and GVR
  // framebuffer to match.
  gvr::Sizei render_target_size =
      gvr_api->GetMaximumEffectiveRenderTargetSize();

  gfx::Size webvr_size(render_target_size.width, render_target_size.height);

  // Ensure that the width is an even number so that the eyes each
  // get the same size, the recommended renderWidth is per eye
  // and the client will use the sum of the left and right width.
  //
  // TODO(klausw,crbug.com/699350): should we round the recommended
  // size to a multiple of 2^N pixels to be friendlier to the GPU? The
  // exact size doesn't matter, and it might be more efficient.
  webvr_size.set_width(webvr_size.width() & ~1);
  return webvr_size;
}

mojom::VREyeParametersPtr CreateEyeParamater(
    gvr::GvrApi* gvr_api,
    gvr::Eye eye,
    const gvr::BufferViewportList& buffers,
    const gfx::Size& maximum_size) {
  mojom::VREyeParametersPtr eye_params = mojom::VREyeParameters::New();
  eye_params->fieldOfView = mojom::VRFieldOfView::New();
  eye_params->offset.resize(3);
  eye_params->renderWidth = maximum_size.width() / 2;
  eye_params->renderHeight = maximum_size.height();

  gvr::BufferViewport eye_viewport = gvr_api->CreateBufferViewport();
  buffers.GetBufferViewport(eye, &eye_viewport);
  gvr::Rectf eye_fov = eye_viewport.GetSourceFov();
  eye_params->fieldOfView->upDegrees = eye_fov.top;
  eye_params->fieldOfView->downDegrees = eye_fov.bottom;
  eye_params->fieldOfView->leftDegrees = eye_fov.left;
  eye_params->fieldOfView->rightDegrees = eye_fov.right;

  gvr::Mat4f eye_mat = gvr_api->GetEyeFromHeadMatrix(eye);
  eye_params->offset[0] = -eye_mat.m[0][3];
  eye_params->offset[1] = -eye_mat.m[1][3];
  eye_params->offset[2] = -eye_mat.m[2][3];
  return eye_params;
}

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(gvr::GvrApi* gvr_api,
                                            mojom::XRDeviceId device_id) {
  TRACE_EVENT0("input", "GvrDelegate::CreateVRDisplayInfo");

  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();

  device->id = device_id;

  device->capabilities = mojom::VRDisplayCapabilities::New();
  device->capabilities->hasPosition = false;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = true;

  std::string vendor = gvr_api->GetViewerVendor();
  std::string model = gvr_api->GetViewerModel();
  device->displayName = vendor + " " + model;

  gvr::BufferViewportList gvr_buffer_viewports =
      gvr_api->CreateEmptyBufferViewportList();
  gvr_buffer_viewports.SetToRecommendedBufferViewports();

  gfx::Size maximum_size = GetMaximumWebVrSize(gvr_api);
  device->leftEye = CreateEyeParamater(gvr_api, GVR_LEFT_EYE,
                                       gvr_buffer_viewports, maximum_size);
  device->rightEye = CreateEyeParamater(gvr_api, GVR_RIGHT_EYE,
                                        gvr_buffer_viewports, maximum_size);

  // This scalar will be applied in the renderer to the recommended render
  // target sizes. For WebVR it will always be applied, for WebXR it can be
  // overridden.
  if (base::AndroidHardwareBufferCompat::IsSupportAvailable()) {
    device->webxr_default_framebuffer_scale = kWebXrRecommendedResolutionScale;
  } else {
    device->webxr_default_framebuffer_scale =
        kWebXrNoSharedBufferResolutionScale;
  }
  device->webvr_default_framebuffer_scale = kWebVrRecommendedResolutionScale;

  return device;
}

}  // namespace

GvrDevice::GvrDevice()
    : VRDeviceBase(mojom::XRDeviceId::GVR_DEVICE_ID),
      exclusive_controller_binding_(this),
      weak_ptr_factory_(this) {
  GvrDelegateProviderFactory::SetDevice(this);
}

GvrDevice::~GvrDevice() {
  if (HasExclusiveSession()) {
    // We potentially could be destroyed during a navigation before processing
    // the exclusive session connection error handler.  In this case, the
    // delegate thinks we are still presenting.
    StopPresenting();
  }

  GvrDelegateProviderFactory::SetDevice(nullptr);
  if (!non_presenting_context_.obj())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NonPresentingGvrContext_shutdown(env, non_presenting_context_);
}

void GvrDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  if (!gvr_api_) {
    EnsureGvrReady();
    if (!gvr_api_) {
      std::move(callback).Run(nullptr, nullptr);
      return;
    }
  }

  if (!options->immersive) {
    // TODO(https://crbug.com/695937): This should be NOTREACHED() once we no
    // longer need the hacked GRV non-immersive mode.  This should now only be
    // hit if orientation devices are disabled by flag.
    ReturnNonImmersiveSession(std::move(callback));
    return;
  }

  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider) {
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  // StartWebXRPresentation is async as we may trigger a DON (Device ON) flow
  // that pauses Chrome.
  delegate_provider->StartWebXRPresentation(
      GetVRDisplayInfo(), std::move(options),
      base::BindOnce(&GvrDevice::OnStartPresentResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GvrDevice::OnStartPresentResult(
    mojom::XRRuntime::RequestSessionCallback callback,
    mojom::XRSessionPtr session) {
  if (!session) {
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  OnStartPresenting();

  mojom::XRSessionControllerPtr session_controller;
  // Close the binding to ensure any previous sessions were closed.
  // TODO(billorr): Only do this in OnPresentingControllerMojoConnectionError.
  exclusive_controller_binding_.Close();
  exclusive_controller_binding_.Bind(mojo::MakeRequest(&session_controller));

  // Unretained is safe because the error handler won't be called after the
  // binding has been destroyed.
  exclusive_controller_binding_.set_connection_error_handler(
      base::BindOnce(&GvrDevice::OnPresentingControllerMojoConnectionError,
                     base::Unretained(this)));

  std::move(callback).Run(std::move(session), std::move(session_controller));
}

// XRSessionController
void GvrDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  DCHECK(false);
}

void GvrDevice::OnPresentingControllerMojoConnectionError() {
  StopPresenting();
}

void GvrDevice::StopPresenting() {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (delegate_provider)
    delegate_provider->ExitWebVRPresent();
  OnExitPresent();
  exclusive_controller_binding_.Close();
}

void GvrDevice::EnsureGvrReady() {
  if (!non_presenting_context_.obj() || !gvr_api_) {
    GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
    if (!delegate_provider || delegate_provider->ShouldDisableGvrDevice())
      return;
    JNIEnv* env = base::android::AttachCurrentThread();
    non_presenting_context_.Reset(Java_NonPresentingGvrContext_create(
        env, reinterpret_cast<jlong>(this)));
    if (!non_presenting_context_.obj())
      return;
    jlong context = Java_NonPresentingGvrContext_getNativeGvrContext(
        env, non_presenting_context_);
    gvr_api_ =
        gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(context));
    SetVRDisplayInfo(CreateVRDisplayInfo(gvr_api_.get(), GetId()));

    if (paused_) {
      PauseTracking();
    } else {
      ResumeTracking();
    }
  }
}

void GvrDevice::OnMagicWindowFrameDataRequest(
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  if (!gvr_api_) {
    std::move(callback).Run(nullptr);
    return;
  }
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
  frame_data->pose =
      GvrDelegate::GetVRPosePtrWithNeckModel(gvr_api_.get(), nullptr);
  std::move(callback).Run(std::move(frame_data));
}

void GvrDevice::OnListeningForActivate(bool listening) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider)
    return;
  delegate_provider->OnListeningForActivateChanged(listening);
}

void GvrDevice::PauseTracking() {
  paused_ = true;
  if (gvr_api_ && non_presenting_context_.obj()) {
    gvr_api_->PauseTracking();
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_NonPresentingGvrContext_pause(env, non_presenting_context_);
  }
}

void GvrDevice::ResumeTracking() {
  paused_ = false;
  if (gvr_api_ && non_presenting_context_.obj()) {
    gvr_api_->ResumeTracking();
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_NonPresentingGvrContext_resume(env, non_presenting_context_);
  }
}

void GvrDevice::EnsureInitialized(EnsureInitializedCallback callback) {
  EnsureGvrReady();
  std::move(callback).Run();
}

GvrDelegateProvider* GvrDevice::GetGvrDelegateProvider() {
  // GvrDelegateProviderFactory::Create() may fail transiently, so every time we
  // try to get it, set the device ID.
  GvrDelegateProvider* delegate_provider = GvrDelegateProviderFactory::Create();
  if (delegate_provider)
    delegate_provider->SetDeviceId(GetId());
  return delegate_provider;
}

void GvrDevice::OnDisplayConfigurationChanged(JNIEnv* env,
                                              const JavaRef<jobject>& obj) {
  DCHECK(gvr_api_);
  SetVRDisplayInfo(CreateVRDisplayInfo(gvr_api_.get(), GetId()));
}

void GvrDevice::Activate(mojom::VRDisplayEventReason reason,
                         base::Callback<void(bool)> on_handled) {
  OnActivate(reason, std::move(on_handled));
}

}  // namespace device
