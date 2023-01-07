// Copyright 2016 The Chromium Authors
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
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/android/gvr/gvr_utils.h"
#include "device/vr/jni_headers/NonPresentingGvrContext_jni.h"
#include "device/vr/util/transform_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"

using base::android::JavaRef;

namespace device {

namespace {

const std::vector<mojom::XRSessionFeature>& GetSupportedFeatures() {
  static base::NoDestructor<std::vector<mojom::XRSessionFeature>>
      kSupportedFeatures{{
    mojom::XRSessionFeature::REF_SPACE_VIEWER,
    mojom::XRSessionFeature::REF_SPACE_LOCAL,
    mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
  }};

  return *kSupportedFeatures;
}

}  // namespace

GvrDevice::GvrDevice() : VRDeviceBase(mojom::XRDeviceId::GVR_DEVICE_ID) {
  GvrDelegateProviderFactory::SetDevice(this);

  SetSupportedFeatures(GetSupportedFeatures());
}

GvrDevice::~GvrDevice() {
  if (HasExclusiveSession()) {
    // We potentially could be destroyed during a navigation before processing
    // the exclusive session connection error handler.  In this case, the
    // delegate thinks we are still presenting.
    StopPresenting();
  }

  if (pending_request_session_callback_) {
    std::move(pending_request_session_callback_).Run(nullptr);
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
    std::move(callback).Run(nullptr);
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
    std::move(pending_request_session_callback_).Run(nullptr);
    return;
  }

  OnStartPresenting();

  // Close the binding to ensure any previous sessions were closed.
  // TODO(billorr): Only do this in OnPresentingControllerMojoConnectionError.
  exclusive_controller_receiver_.reset();

  auto session_result = mojom::XRRuntimeSessionResult::New();
  session_result->controller =
      exclusive_controller_receiver_.BindNewPipeAndPassRemote();
  session_result->session = std::move(session);

  std::move(pending_request_session_callback_).Run(std::move(session_result));

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
    std::move(pending_request_session_callback_).Run(nullptr);
    return;
  }

  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider) {
    std::move(pending_request_session_callback_).Run(nullptr);
    return;
  }

  DCHECK_EQ(options->mode, mojom::XRSessionMode::kImmersiveVr);

  // StartWebXRPresentation is async as we may trigger a DON (Device ON) flow
  // that pauses Chrome.
  delegate_provider->StartWebXRPresentation(
      std::move(options), base::BindOnce(&GvrDevice::OnStartPresentResult,
                                         weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace device
