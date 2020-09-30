// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/browser_xr_runtime_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#if defined(OS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
#endif

#include "base/bind_helpers.h"
#include "base/numerics/ranges.h"
#include "build/build_config.h"
#include "content/browser/xr/service/vr_service_impl.h"
#include "content/browser/xr/xr_utils.h"
#include "content/public/browser/xr_install_helper.h"
#include "content/public/browser/xr_integration_client.h"
#include "content/public/common/content_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/cpp/session_mode.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace content {
namespace {
bool IsValidTransform(const gfx::Transform& transform,
                      float max_translate_meters) {
  if (!transform.IsInvertible() || transform.HasPerspective())
    return false;

  gfx::DecomposedTransform decomp;
  if (!DecomposeTransform(&decomp, transform))
    return false;

  float kEpsilon = 0.1f;
  if (abs(decomp.perspective[3] - 1) > kEpsilon) {
    // If testing with unexpectedly high values, catch on debug builds rather
    // than silently change data.  On release builds its better to be safe and
    // validate.
    DCHECK(false);
    return false;
  }
  for (int i = 0; i < 3; ++i) {
    if (abs(decomp.scale[i] - 1) > kEpsilon)
      return false;
    if (abs(decomp.skew[i]) > kEpsilon)
      return false;
    if (abs(decomp.perspective[i]) > kEpsilon)
      return false;
    if (abs(decomp.translate[i]) > max_translate_meters)
      return false;
  }

  // Only rotate and translate.
  return true;
}

device::mojom::VREyeParametersPtr ValidateEyeParameters(
    const device::mojom::VREyeParameters* eye) {
  if (!eye)
    return nullptr;
  device::mojom::VREyeParametersPtr ret = device::mojom::VREyeParameters::New();
  // FOV
  float kDefaultFOV = 45;
  ret->field_of_view = device::mojom::VRFieldOfView::New();
  if (eye->field_of_view->up_degrees < 90 &&
      eye->field_of_view->up_degrees > -90 &&
      eye->field_of_view->up_degrees > -eye->field_of_view->down_degrees &&
      eye->field_of_view->down_degrees < 90 &&
      eye->field_of_view->down_degrees > -90 &&
      eye->field_of_view->down_degrees > -eye->field_of_view->up_degrees &&
      eye->field_of_view->left_degrees < 90 &&
      eye->field_of_view->left_degrees > -90 &&
      eye->field_of_view->left_degrees > -eye->field_of_view->right_degrees &&
      eye->field_of_view->right_degrees < 90 &&
      eye->field_of_view->right_degrees > -90 &&
      eye->field_of_view->right_degrees > -eye->field_of_view->left_degrees) {
    ret->field_of_view->up_degrees = eye->field_of_view->up_degrees;
    ret->field_of_view->down_degrees = eye->field_of_view->down_degrees;
    ret->field_of_view->left_degrees = eye->field_of_view->left_degrees;
    ret->field_of_view->right_degrees = eye->field_of_view->right_degrees;
  } else {
    ret->field_of_view->up_degrees = kDefaultFOV;
    ret->field_of_view->down_degrees = kDefaultFOV;
    ret->field_of_view->left_degrees = kDefaultFOV;
    ret->field_of_view->right_degrees = kDefaultFOV;
  }

  // Head-from-Eye Transform
  // Maximum 10m translation.
  if (IsValidTransform(eye->head_from_eye, 10)) {
    ret->head_from_eye = eye->head_from_eye;
  }
  // else, ret->head_from_eye remains the identity transform

  // Renderwidth/height
  uint32_t kMaxSize = 16384;
  uint32_t kMinSize = 2;
  // DCHECK on debug builds to catch legitimate large sizes, but clamp on
  // release builds to ensure valid state.
  DCHECK(eye->render_width < kMaxSize);
  DCHECK(eye->render_height < kMaxSize);
  ret->render_width = base::ClampToRange(eye->render_width, kMinSize, kMaxSize);
  ret->render_height =
      base::ClampToRange(eye->render_height, kMinSize, kMaxSize);
  return ret;
}

device::mojom::VRDisplayInfoPtr ValidateVRDisplayInfo(
    const device::mojom::VRDisplayInfo* info) {
  if (!info)
    return nullptr;

  device::mojom::VRDisplayInfoPtr ret = device::mojom::VRDisplayInfo::New();

  // Maximum 1000km translation.
  if (info->stage_parameters &&
      IsValidTransform(info->stage_parameters->mojo_from_floor, 1000000)) {
    ret->stage_parameters = device::mojom::VRStageParameters::New(
        info->stage_parameters->mojo_from_floor,
        info->stage_parameters->bounds);
  }

  ret->left_eye = ValidateEyeParameters(info->left_eye.get());
  ret->right_eye = ValidateEyeParameters(info->right_eye.get());
  return ret;
}

// TODO(crbug.com/995377): Report these from the device runtime instead.
constexpr device::mojom::XRSessionFeature kOrientationDeviceFeatures[] = {
    device::mojom::XRSessionFeature::REF_SPACE_VIEWER,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
};

constexpr device::mojom::XRSessionFeature kGVRDeviceFeatures[] = {
    device::mojom::XRSessionFeature::REF_SPACE_VIEWER,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
};

constexpr device::mojom::XRSessionFeature kARCoreDeviceFeatures[] = {
    device::mojom::XRSessionFeature::REF_SPACE_VIEWER,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
    device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED,
    device::mojom::XRSessionFeature::DOM_OVERLAY,
    device::mojom::XRSessionFeature::LIGHT_ESTIMATION,
    device::mojom::XRSessionFeature::ANCHORS,
    device::mojom::XRSessionFeature::PLANE_DETECTION,
    device::mojom::XRSessionFeature::DEPTH,
};

#if BUILDFLAG(ENABLE_WINDOWS_MR)
constexpr device::mojom::XRSessionFeature kWindowsMixedRealityFeatures[] = {
    device::mojom::XRSessionFeature::REF_SPACE_VIEWER,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
    device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR,
};
#endif

#if BUILDFLAG(ENABLE_OPENXR)
constexpr device::mojom::XRSessionFeature kOpenXRFeatures[] = {
    device::mojom::XRSessionFeature::REF_SPACE_VIEWER,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
    device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR,
    device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED,
};
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
constexpr device::mojom::XRSessionFeature kOculusFeatures[] = {
    device::mojom::XRSessionFeature::REF_SPACE_VIEWER,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
    device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR,
};
#endif

bool ContainsFeature(
    base::span<const device::mojom::XRSessionFeature> feature_list,
    device::mojom::XRSessionFeature feature) {
  return std::find(feature_list.begin(), feature_list.end(), feature) !=
         feature_list.end();
}
}  // anonymous namespace

BrowserXRRuntimeImpl::BrowserXRRuntimeImpl(
    device::mojom::XRDeviceId id,
    device::mojom::XRDeviceDataPtr device_data,
    mojo::PendingRemote<device::mojom::XRRuntime> runtime,
    device::mojom::VRDisplayInfoPtr display_info)
    : id_(id),
      device_data_(std::move(device_data)),
      runtime_(std::move(runtime)),
      display_info_(ValidateVRDisplayInfo(display_info.get())) {
  DVLOG(2) << __func__ << ": id=" << id;
  // Unretained is safe because we are calling through an InterfacePtr we own,
  // so we won't be called after runtime_ is destroyed.
  runtime_->ListenToDeviceChanges(
      receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&BrowserXRRuntimeImpl::OnDisplayInfoChanged,
                     base::Unretained(this)));

  // TODO(crbug.com/1031622): Convert this to a query for the client off of
  // ContentBrowserClient once BrowserXRRuntimeImpl moves to content.
  auto* integration_client = GetXrIntegrationClient();

  if (integration_client) {
    install_helper_ = integration_client->GetInstallHelper(id_);
  }
}

BrowserXRRuntimeImpl::~BrowserXRRuntimeImpl() {
  DVLOG(2) << __func__ << ": id=" << id_;

  if (install_finished_callback_) {
    std::move(install_finished_callback_).Run(false);
  }
}

void BrowserXRRuntimeImpl::ExitActiveImmersiveSession() {
  DVLOG(2) << __func__;
  auto* service = GetServiceWithActiveImmersiveSession();
  if (service) {
    service->ExitPresent(base::DoNothing());
  }
}

bool BrowserXRRuntimeImpl::SupportsFeature(
    device::mojom::XRSessionFeature feature) const {
  switch (id_) {
    // Test/fake devices support all features.
    case device::mojom::XRDeviceId::WEB_TEST_DEVICE_ID:
    case device::mojom::XRDeviceId::FAKE_DEVICE_ID:
      return true;
    case device::mojom::XRDeviceId::ARCORE_DEVICE_ID:
      // Only support hit test if the feature flag is enabled.
      if (feature == device::mojom::XRSessionFeature::HIT_TEST) {
        return base::FeatureList::IsEnabled(features::kWebXrHitTest);
      }

#if defined(OS_ANDROID)
      // Only support camera access if the feature flag is enabled & the device
      // supports shared buffers.
      if (feature == device::mojom::XRSessionFeature::CAMERA_ACCESS) {
        return base::FeatureList::IsEnabled(features::kWebXrIncubations) &&
               base::AndroidHardwareBufferCompat::IsSupportAvailable();
      }
#endif

      return ContainsFeature(kARCoreDeviceFeatures, feature);
    case device::mojom::XRDeviceId::ORIENTATION_DEVICE_ID:
      return ContainsFeature(kOrientationDeviceFeatures, feature);
    case device::mojom::XRDeviceId::GVR_DEVICE_ID:
      return ContainsFeature(kGVRDeviceFeatures, feature);

#if BUILDFLAG(ENABLE_OCULUS_VR)
    case device::mojom::XRDeviceId::OCULUS_DEVICE_ID:
      return ContainsFeature(kOculusFeatures, feature);
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    case device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID:
      return ContainsFeature(kWindowsMixedRealityFeatures, feature);
#endif

#if BUILDFLAG(ENABLE_OPENXR)
    case device::mojom::XRDeviceId::OPENXR_DEVICE_ID:
      return ContainsFeature(kOpenXRFeatures, feature);
#endif
  }

  NOTREACHED();
}

bool BrowserXRRuntimeImpl::SupportsAllFeatures(
    const std::vector<device::mojom::XRSessionFeature>& features) const {
  for (const auto& feature : features) {
    if (!SupportsFeature(feature))
      return false;
  }

  return true;
}

bool BrowserXRRuntimeImpl::SupportsCustomIPD() const {
  switch (id_) {
    case device::mojom::XRDeviceId::ARCORE_DEVICE_ID:
    case device::mojom::XRDeviceId::WEB_TEST_DEVICE_ID:
    case device::mojom::XRDeviceId::FAKE_DEVICE_ID:
    case device::mojom::XRDeviceId::ORIENTATION_DEVICE_ID:
    case device::mojom::XRDeviceId::GVR_DEVICE_ID:
      return false;
#if BUILDFLAG(ENABLE_OCULUS_VR)
    case device::mojom::XRDeviceId::OCULUS_DEVICE_ID:
      return true;
#endif
#if BUILDFLAG(ENABLE_WINDOWS_MR)
    case device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID:
      return true;
#endif
#if BUILDFLAG(ENABLE_OPENXR)
    case device::mojom::XRDeviceId::OPENXR_DEVICE_ID:
      return true;
#endif
  }

  NOTREACHED();
}

bool BrowserXRRuntimeImpl::SupportsNonEmulatedHeight() const {
  switch (id_) {
    case device::mojom::XRDeviceId::ARCORE_DEVICE_ID:
    case device::mojom::XRDeviceId::WEB_TEST_DEVICE_ID:
    case device::mojom::XRDeviceId::FAKE_DEVICE_ID:
    case device::mojom::XRDeviceId::ORIENTATION_DEVICE_ID:
      return false;
    case device::mojom::XRDeviceId::GVR_DEVICE_ID:
#if BUILDFLAG(ENABLE_OCULUS_VR)
    case device::mojom::XRDeviceId::OCULUS_DEVICE_ID:
#endif
#if BUILDFLAG(ENABLE_WINDOWS_MR)
    case device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID:
#endif
#if BUILDFLAG(ENABLE_OPENXR)
    case device::mojom::XRDeviceId::OPENXR_DEVICE_ID:
#endif
      return true;
  }

  NOTREACHED();
}

void BrowserXRRuntimeImpl::OnDisplayInfoChanged(
    device::mojom::VRDisplayInfoPtr vr_device_info) {
  bool had_display_info = !!display_info_;
  display_info_ = ValidateVRDisplayInfo(vr_device_info.get());
  if (had_display_info) {
    for (VRServiceImpl* service : services_) {
      service->OnDisplayInfoChanged();
    }
  }

  // Notify observers of the new display info.
  for (Observer& observer : observers_) {
    observer.SetVRDisplayInfo(display_info_.Clone());
  }
}

void BrowserXRRuntimeImpl::StopImmersiveSession(
    VRServiceImpl::ExitPresentCallback on_exited) {
  DVLOG(2) << __func__;
  if (immersive_session_controller_) {
    immersive_session_controller_.reset();
    if (presenting_service_) {
      presenting_service_->OnExitPresent();
      presenting_service_ = nullptr;
    }

    for (Observer& observer : observers_) {
      observer.SetWebXRWebContents(nullptr);
    }
  }
  std::move(on_exited).Run();
}

void BrowserXRRuntimeImpl::OnExitPresent() {
  DVLOG(2) << __func__;
  if (presenting_service_) {
    presenting_service_->OnExitPresent();
    presenting_service_ = nullptr;
  }
}

void BrowserXRRuntimeImpl::OnVisibilityStateChanged(
    device::mojom::XRVisibilityState visibility_state) {
  for (VRServiceImpl* service : services_) {
    service->OnVisibilityStateChanged(visibility_state);
  }
}

void BrowserXRRuntimeImpl::OnServiceAdded(VRServiceImpl* service) {
  DVLOG(2) << __func__ << ": id=" << id_;
  services_.insert(service);
}

void BrowserXRRuntimeImpl::OnServiceRemoved(VRServiceImpl* service) {
  DVLOG(2) << __func__ << ": id=" << id_;
  DCHECK(service);
  services_.erase(service);
  if (service == presenting_service_) {
    ExitPresent(service, base::DoNothing());
  }
}

void BrowserXRRuntimeImpl::ExitPresent(
    VRServiceImpl* service,
    VRServiceImpl::ExitPresentCallback on_exited) {
  DVLOG(2) << __func__ << ": id=" << id_ << " service=" << service
           << " presenting_service_=" << presenting_service_;
  if (service == presenting_service_) {
    runtime_->ShutdownSession(
        base::BindOnce(&BrowserXRRuntimeImpl::StopImmersiveSession,
                       weak_ptr_factory_.GetWeakPtr(), std::move(on_exited)));
  }
}

void BrowserXRRuntimeImpl::SetFramesThrottled(const VRServiceImpl* service,
                                              bool throttled) {
  if (service == presenting_service_) {
    for (Observer& observer : observers_) {
      observer.SetFramesThrottled(throttled);
    }
  }
}

void BrowserXRRuntimeImpl::RequestSession(
    VRServiceImpl* service,
    const device::mojom::XRRuntimeSessionOptionsPtr& options,
    RequestSessionCallback callback) {
  DVLOG(2) << __func__ << ": id=" << id_;
  // base::Unretained is safe because we won't be called back after runtime_ is
  // destroyed.
  runtime_->RequestSession(
      options->Clone(),
      base::BindOnce(&BrowserXRRuntimeImpl::OnRequestSessionResult,
                     base::Unretained(this), service->GetWeakPtr(),
                     options->Clone(), std::move(callback)));
}

void BrowserXRRuntimeImpl::OnRequestSessionResult(
    base::WeakPtr<VRServiceImpl> service,
    device::mojom::XRRuntimeSessionOptionsPtr options,
    RequestSessionCallback callback,
    device::mojom::XRSessionPtr session,
    mojo::PendingRemote<device::mojom::XRSessionController>
        immersive_session_controller) {
  if (session && service) {
    DVLOG(2) << __func__ << ": id=" << id_;
    if (device::XRSessionModeUtils::IsImmersive(options->mode)) {
      presenting_service_ = service.get();
      immersive_session_controller_.Bind(
          std::move(immersive_session_controller));
      immersive_session_controller_.set_disconnect_handler(
          base::BindOnce(&BrowserXRRuntimeImpl::OnImmersiveSessionError,
                         base::Unretained(this)));

      // Notify observers that we have started presentation.
      content::WebContents* web_contents = service->GetWebContents();
      for (Observer& observer : observers_) {
        observer.SetWebXRWebContents(web_contents);
      }
    }

    std::move(callback).Run(std::move(session));
  } else {
    std::move(callback).Run(nullptr);
    if (session) {
      // The service has been removed, but we still got a session, so make
      // sure to clean up this weird state.
      immersive_session_controller_.Bind(
          std::move(immersive_session_controller));
      StopImmersiveSession(base::DoNothing());
    }
  }
}

void BrowserXRRuntimeImpl::EnsureInstalled(
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> install_callback) {
  // If there's no install helper, then we can assume no install is needed.
  if (!install_helper_) {
    std::move(install_callback).Run(true);
    return;
  }

  // Only the most recent caller will be notified of a successful install.
  bool had_outstanding_callback = false;
  if (install_finished_callback_) {
    had_outstanding_callback = true;
    std::move(install_finished_callback_).Run(false);
  }

  install_finished_callback_ = std::move(install_callback);

  // If we already had a cached install callback, then we don't need to query
  // for installation again.
  if (had_outstanding_callback)
    return;

  install_helper_->EnsureInstalled(
      render_process_id, render_frame_id,
      base::BindOnce(&BrowserXRRuntimeImpl::OnInstallFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowserXRRuntimeImpl::OnInstallFinished(bool succeeded) {
  DCHECK(install_finished_callback_);

  std::move(install_finished_callback_).Run(succeeded);
}

void BrowserXRRuntimeImpl::OnImmersiveSessionError() {
  DVLOG(2) << __func__ << ": id=" << id_;
  StopImmersiveSession(base::DoNothing());
}

void BrowserXRRuntimeImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  observer->SetVRDisplayInfo(display_info_.Clone());
}

void BrowserXRRuntimeImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BrowserXRRuntimeImpl::BeforeRuntimeRemoved() {
  DVLOG(1) << __func__ << ": id=" << id_;

  // If the device process crashes or otherwise gets removed, it's a race as to
  // whether or not our mojo interface to the device gets reset before we're
  // deleted as the result of the device provider being destroyed.
  // Since this no-ops if we don't have an active immersive session, try to end
  // any immersive session we may be currently responsible for.
  StopImmersiveSession(base::DoNothing());
}

#if defined(OS_WIN)
base::Optional<LUID> BrowserXRRuntimeImpl::GetLuid() const {
  return device_data_->luid;
}
#endif

}  // namespace content
