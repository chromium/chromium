// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device.h"

#include <math.h>
#include <algorithm>
#include <string>
#include <utility>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/android/gvr/gvr_utils.h"
#include "device/vr/jni_headers/NonPresentingGvrContext_jni.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

using base::android::JavaRef;

namespace device {

namespace {

// Default downscale factor for computing the recommended WebXR
// render_width/render_height from the 1:1 pixel mapped size. Using a rather
// aggressive downscale due to the high overhead of copying pixels
// twice before handing off to GVR. For comparison, the polyfill
// uses approximately 0.55 on a Pixel XL.
static constexpr float kWebXrRecommendedResolutionScale = 0.7;

// The scale factor for WebXR on devices that don't have shared buffer
// support. (Android N and earlier.)
static constexpr float kWebXrNoSharedBufferResolutionScale = 0.5;

gfx::Size GetMaximumWebVrSize(gvr::GvrApi* gvr_api) {
  // Get the default, unscaled size for the WebVR transfer surface
  // based on the optimal 1:1 render resolution. A scalar will be applied to
  // this value in the renderer to reduce the render load. This size will also
  // be reported to the client via CreateVRDisplayInfo as the
  // client-recommended render_width/render_height and for the GVR
  // framebuffer. If the client chooses a different size or resizes it
  // while presenting, we'll resize the transfer surface and GVR
  // framebuffer to match.
  gvr::Sizei render_target_size =
      gvr_api->GetMaximumEffectiveRenderTargetSize();

  gfx::Size webvr_size(render_target_size.width, render_target_size.height);

  // Ensure that the width is an even number so that the eyes each
  // get the same size, the recommended render_width is per eye
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
  eye_params->field_of_view = mojom::VRFieldOfView::New();
  eye_params->render_width = maximum_size.width() / 2;
  eye_params->render_height = maximum_size.height();

  gvr::BufferViewport eye_viewport = gvr_api->CreateBufferViewport();
  buffers.GetBufferViewport(eye, &eye_viewport);
  gvr::Rectf eye_fov = eye_viewport.GetSourceFov();
  eye_params->field_of_view->up_degrees = eye_fov.top;
  eye_params->field_of_view->down_degrees = eye_fov.bottom;
  eye_params->field_of_view->left_degrees = eye_fov.left;
  eye_params->field_of_view->right_degrees = eye_fov.right;

  gvr::Mat4f eye_mat = gvr_api->GetEyeFromHeadMatrix(eye);
  gfx::Transform eye_from_head;
  gvr_utils::GvrMatToTransform(eye_mat, &eye_from_head);
  DCHECK(eye_from_head.IsInvertible());
  gfx::Transform head_from_eye;
  if (eye_from_head.GetInverse(&head_from_eye)) {
    eye_params->head_from_eye = head_from_eye;
  }

  return eye_params;
}

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(gvr::GvrApi* gvr_api,
                                            mojom::XRDeviceId device_id) {
  TRACE_EVENT0("input", "GvrDelegate::CreateVRDisplayInfo");

  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();

  device->id = device_id;

  gvr::BufferViewportList gvr_buffer_viewports =
      gvr_api->CreateEmptyBufferViewportList();
  gvr_buffer_viewports.SetToRecommendedBufferViewports();

  gfx::Size maximum_size = GetMaximumWebVrSize(gvr_api);
  device->left_eye = CreateEyeParamater(gvr_api, GVR_LEFT_EYE,
                                        gvr_buffer_viewports, maximum_size);
  device->right_eye = CreateEyeParamater(gvr_api, GVR_RIGHT_EYE,
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

  return device;
}

}  // namespace

GvrDevice::GvrDevice() : VRDeviceBase(mojom::XRDeviceId::GVR_DEVICE_ID) {
  GvrDelegateProviderFactory::SetDevice(this);
}

GvrDevice::~GvrDevice() {
  if (HasExclusiveSession()) {
    // We potentially could be destroyed during a navigation before processing
    // the exclusive session connection error handler.  In this case, the
    // delegate thinks we are still presenting.
    StopPresenting();
  }

  if (pending_request_session_callback_) {
    std::move(pending_request_session_callback_)
        .Run(nullptr, mojo::NullRemote());
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
  // We can only process one request at a time.
  if (pending_request_session_callback_) {
    std::move(callback).Run(nullptr, mojo::NullRemote());
    return;
  }
  pending_request_session_callback_ = std::move(callback);

  if (!gvr_api_) {
    Init(base::BindOnce(&GvrDevice::OnInitRequestSessionFinished,
                        base::Unretained(this), std::move(options)));
    return;
  }
  OnInitRequestSessionFinished(std::move(options), true);
}

void GvrDevice::OnStartPresentResult(
    mojom::XRSessionPtr session) {
  DCHECK(pending_request_session_callback_);

  if (!session) {
    std::move(pending_request_session_callback_)
        .Run(nullptr, mojo::NullRemote());
    return;
  }

  OnStartPresenting();

  // Close the binding to ensure any previous sessions were closed.
  // TODO(billorr): Only do this in OnPresentingControllerMojoConnectionError.
  exclusive_controller_receiver_.reset();

  std::move(pending_request_session_callback_)
      .Run(std::move(session),
           exclusive_controller_receiver_.BindNewPipeAndPassRemote());

  // Unretained is safe because the error handler won't be called after the
  // binding has been destroyed.
  exclusive_controller_receiver_.set_disconnect_handler(
      base::BindOnce(&GvrDevice::OnPresentingControllerMojoConnectionError,
                     base::Unretained(this)));
}

// XRSessionController
void GvrDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  DCHECK(false);
}

void GvrDevice::OnPresentingControllerMojoConnectionError() {
  StopPresenting();
}

void GvrDevice::ShutdownSession(
    mojom::XRRuntime::ShutdownSessionCallback on_completed) {
  DVLOG(2) << __func__;
  StopPresenting();

  // At this point, the main thread session shutdown is complete, but the GL
  // thread may still be in the process of finishing shutdown or transitioning
  // to VR Browser mode. Java VrShell::setWebVrModeEnable calls native
  // VrShell::setWebVrMode which calls BrowserRenderer::SetWebXrMode on the GL
  // thread, and that triggers the VRB transition via ui_->SetWebVrMode.
  //
  // Since tasks posted to the GL thread are handled in sequence, any calls
  // related to a new session will be processed after the GL thread transition
  // is complete.
  //
  // TODO(https://crbug.com/998307): It would be cleaner to delay the shutdown
  // until the GL thread transition is complete, but this would need a fair
  // amount of additional plumbing to ensure that the callback is consistently
  // called. See also WebXrTestFramework.enterSessionWithUserGesture(), but
  // it's unclear if changing this would be sufficient to avoid the need for
  // workarounds there.
  std::move(on_completed).Run();
}

void GvrDevice::StopPresenting() {
  DVLOG(2) << __func__;
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (delegate_provider)
    delegate_provider->ExitWebVRPresent();
  OnExitPresent();
  exclusive_controller_receiver_.reset();
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

GvrDelegateProvider* GvrDevice::GetGvrDelegateProvider() {
  // GvrDelegateProviderFactory::Create() may return a different
  // pointer each time. Do not cache it.
  return GvrDelegateProviderFactory::Create();
}

void GvrDevice::OnDisplayConfigurationChanged(JNIEnv* env,
                                              const JavaRef<jobject>& obj) {
  DCHECK(gvr_api_);
  SetVRDisplayInfo(CreateVRDisplayInfo(gvr_api_.get(), GetId()));
}

void GvrDevice::Init(base::OnceCallback<void(bool)> on_finished) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider || delegate_provider->ShouldDisableGvrDevice()) {
    std::move(on_finished).Run(false);
    return;
  }
  CreateNonPresentingContext();
  std::move(on_finished).Run(non_presenting_context_.obj() != nullptr);
}

void GvrDevice::CreateNonPresentingContext() {
  if (non_presenting_context_.obj())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  non_presenting_context_.Reset(
      Java_NonPresentingGvrContext_create(env, reinterpret_cast<jlong>(this)));
  if (!non_presenting_context_.obj())
    return;
  jlong context = Java_NonPresentingGvrContext_getNativeGvrContext(
      env, non_presenting_context_);
  gvr_api_ = gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(context));
  SetVRDisplayInfo(CreateVRDisplayInfo(gvr_api_.get(), GetId()));

  if (paused_) {
    PauseTracking();
  } else {
    ResumeTracking();
  }
}

void GvrDevice::OnInitRequestSessionFinished(
    mojom::XRRuntimeSessionOptionsPtr options,
    bool success) {
  DCHECK(pending_request_session_callback_);

  if (!success) {
    std::move(pending_request_session_callback_)
        .Run(nullptr, mojo::NullRemote());
    return;
  }

  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider) {
    std::move(pending_request_session_callback_)
        .Run(nullptr, mojo::NullRemote());
    return;
  }

  DCHECK_EQ(options->mode, mojom::XRSessionMode::kImmersiveVr);

  // StartWebXRPresentation is async as we may trigger a DON (Device ON) flow
  // that pauses Chrome.
  delegate_provider->StartWebXRPresentation(
      GetVRDisplayInfo(), std::move(options),
      base::BindOnce(&GvrDevice::OnStartPresentResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace device
