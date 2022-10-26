// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/browser_xr_runtime_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/browser/xr/service/vr_service_impl.h"
#include "content/browser/xr/xr_utils.h"
#include "content/public/browser/browser_xr_runtime.h"
#include "content/public/browser/xr_install_helper.h"
#include "content/public/browser/xr_integration_client.h"
#include "content/public/common/content_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/cpp/session_mode.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/transform.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
#endif

namespace content {
namespace {
bool IsValidTransform(const gfx::Transform& transform) {
  if (!transform.IsInvertible() || transform.HasPerspective())
    return false;

  absl::optional<gfx::DecomposedTransform> decomp = transform.Decompose();
  if (!decomp)
    return false;

  float kEpsilon = 0.1f;
  if (abs(decomp->perspective[3] - 1) > kEpsilon) {
    // If testing with unexpectedly high values, catch on debug builds rather
    // than silently change data.  On release builds its better to be safe and
    // validate.
    DCHECK(false);
    return false;
  }
  for (int i = 0; i < 3; ++i) {
    if (abs(decomp->scale[i] - 1) > kEpsilon)
      return false;
    if (abs(decomp->skew[i]) > kEpsilon)
      return false;
    if (abs(decomp->perspective[i]) > kEpsilon)
      return false;
  }

  // Only rotate and translate.
  return true;
}

device::mojom::XRViewPtr ValidateXRView(const device::mojom::XRView* view) {
  if (!view) {
    return nullptr;
  }
  device::mojom::XRViewPtr ret = device::mojom::XRView::New();
  ret->eye = view->eye;
  // FOV
  float kDefaultFOV = 45;
  ret->field_of_view = device::mojom::VRFieldOfView::New();
  if (view->field_of_view->up_degrees < 90 &&
      view->field_of_view->up_degrees > -90 &&
      view->field_of_view->up_degrees > -view->field_of_view->down_degrees &&
      view->field_of_view->down_degrees < 90 &&
      view->field_of_view->down_degrees > -90 &&
      view->field_of_view->down_degrees > -view->field_of_view->up_degrees &&
      view->field_of_view->left_degrees < 90 &&
      view->field_of_view->left_degrees > -90 &&
      view->field_of_view->left_degrees > -view->field_of_view->right_degrees &&
      view->field_of_view->right_degrees < 90 &&
      view->field_of_view->right_degrees > -90 &&
      view->field_of_view->right_degrees > -view->field_of_view->left_degrees) {
    ret->field_of_view->up_degrees = view->field_of_view->up_degrees;
    ret->field_of_view->down_degrees = view->field_of_view->down_degrees;
    ret->field_of_view->left_degrees = view->field_of_view->left_degrees;
    ret->field_of_view->right_degrees = view->field_of_view->right_degrees;
  } else {
    ret->field_of_view->up_degrees = kDefaultFOV;
    ret->field_of_view->down_degrees = kDefaultFOV;
    ret->field_of_view->left_degrees = kDefaultFOV;
    ret->field_of_view->right_degrees = kDefaultFOV;
  }

  if (IsValidTransform(view->mojo_from_view)) {
    ret->mojo_from_view = view->mojo_from_view;
  }
  // else, ret->mojo_from_view remains the identity transform

  // Renderwidth/height
  int kMaxSize = 16384;
  int kMinSize = 2;
  // DCHECK on debug builds to catch legitimate large sizes, but clamp on
  // release builds to ensure valid state.
  DCHECK_LT(view->viewport.width() + view->viewport.x(), kMaxSize);
  DCHECK_LT(view->viewport.height() + view->viewport.y(), kMaxSize);
  DCHECK_GT(view->viewport.width() + view->viewport.x(), kMinSize);
  DCHECK_GT(view->viewport.height() + view->viewport.y(), kMinSize);
  ret->viewport =
      gfx::Rect(base::clamp(view->viewport.x(), 0, kMaxSize),
                base::clamp(view->viewport.y(), 0, kMaxSize),
                base::clamp(view->viewport.width(), kMinSize, kMaxSize),
                base::clamp(view->viewport.height(), kMinSize, kMaxSize));
  return ret;
}

}  // anonymous namespace

BrowserXRRuntimeImpl::BrowserXRRuntimeImpl(
    device::mojom::XRDeviceId id,
    device::mojom::XRDeviceDataPtr device_data,
    mojo::PendingRemote<device::mojom::XRRuntime> runtime)
    : id_(id),
      device_data_(std::move(device_data)),
      runtime_(std::move(runtime)) {
  DVLOG(2) << __func__ << ": id=" << id;

  runtime_->ListenToDeviceChanges(receiver_.BindNewEndpointAndPassRemote());

  // TODO(crbug.com/1031622): Convert this to a query for the client off of
  // ContentBrowserClient once BrowserXRRuntimeImpl moves to content.
  auto* integration_client = GetXrIntegrationClient();

  if (integration_client) {
    install_helper_ = integration_client->GetInstallHelper(id_);
    runtime_observer_ = integration_client->CreateRuntimeObserver();

    if (runtime_observer_) {
      AddObserver(runtime_observer_.get());
    }
  }
}

BrowserXRRuntimeImpl::~BrowserXRRuntimeImpl() {
  DVLOG(2) << __func__ << ": id=" << id_;

  if (runtime_observer_) {
    RemoveObserver(runtime_observer_.get());
  }

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
  if(id_ == device::mojom::XRDeviceId::WEB_TEST_DEVICE_ID ||
     id_ == device::mojom::XRDeviceId::FAKE_DEVICE_ID)
      return true;

  return base::Contains(device_data_->supported_features, feature);
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
#if BUILDFLAG(ENABLE_OPENXR)
    case device::mojom::XRDeviceId::OPENXR_DEVICE_ID:
#endif
      return true;
  }

  NOTREACHED();
}

bool BrowserXRRuntimeImpl::SupportsArBlendMode() {
  return device_data_->is_ar_blend_mode_supported;
}

void BrowserXRRuntimeImpl::StopImmersiveSession(
    VRServiceImpl::ExitPresentCallback on_exited) {
  DVLOG(2) << __func__;

  if (immersive_session_has_camera_access_) {
    for (Observer& observer : observers_) {
      observer.WebXRCameraInUseChanged(nullptr, false);
    }
    immersive_session_has_camera_access_ = false;
  }

  if (immersive_session_controller_) {
    immersive_session_controller_.reset();
    if (presenting_service_) {
      presenting_service_->OnExitPresent();
      presenting_service_ = nullptr;
    }

    for (Observer& observer : observers_) {
      observer.WebXRWebContentsChanged(nullptr);
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
    presenting_service_ = nullptr;
    // Note that we replicate the logic in ExitPresent because we need to clear
    // our presenting_service_ as it is no longer valid. However, the Runtime
    // may still need to be notified to terminate its session. ExitPresent may
    // be called when the service *is* still valid and would need to be notified
    // of this shutdown.
    runtime_->ShutdownSession(
        base::BindOnce(&BrowserXRRuntimeImpl::StopImmersiveSession,
                       weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
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
      observer.WebXRFramesThrottledChanged(throttled);
    }
  }
}

void BrowserXRRuntimeImpl::RequestInlineSession(
    device::mojom::XRRuntimeSessionOptionsPtr options,
    device::mojom::XRRuntime::RequestSessionCallback callback) {
  runtime_->RequestSession(std::move(options), std::move(callback));
}

void BrowserXRRuntimeImpl::RequestImmersiveSession(
    VRServiceImpl* service,
    device::mojom::XRRuntimeSessionOptionsPtr options,
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
    device::mojom::XRRuntimeSessionResultPtr session_result) {
  if (session_result && service) {
    DVLOG(2) << __func__ << ": id=" << id_;
    if (device::XRSessionModeUtils::IsImmersive(options->mode)) {
      presenting_service_ = service.get();
      immersive_session_controller_.Bind(std::move(session_result->controller));
      immersive_session_controller_.set_disconnect_handler(
          base::BindOnce(&BrowserXRRuntimeImpl::OnImmersiveSessionError,
                         base::Unretained(this)));

      std::vector<device::mojom::XRViewPtr>& views =
          session_result->session->device_config->views;

      for (device::mojom::XRViewPtr& view : views) {
        view = ValidateXRView(view.get());
      }

      // Notify observers that we have started presentation.
      content::WebContents* web_contents = service->GetWebContents();
      for (Observer& observer : observers_) {
        observer.SetDefaultXrViews(views);
        observer.WebXRWebContentsChanged(web_contents);
      }

      immersive_session_has_camera_access_ =
          base::Contains(session_result->session->enabled_features,
                         device::mojom::XRSessionFeature::CAMERA_ACCESS);
      if (immersive_session_has_camera_access_) {
        for (Observer& observer : observers_) {
          observer.WebXRCameraInUseChanged(web_contents, true);
        }
      }
    }

    std::move(callback).Run(std::move(session_result));
  } else {
    std::move(callback).Run(nullptr);
    if (session_result) {
      // The service has been removed, but we still got a session, so make
      // sure to clean up this weird state.
      immersive_session_controller_.Bind(std::move(session_result->controller));
      StopImmersiveSession(base::DoNothing());
    }
  }
}

void BrowserXRRuntimeImpl::EnsureInstalled(
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> install_callback) {
  DVLOG(2) << __func__;

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

#if BUILDFLAG(IS_WIN)
absl::optional<CHROME_LUID> BrowserXRRuntimeImpl::GetLuid() const {
  return device_data_->luid;
}
#endif

}  // namespace content
