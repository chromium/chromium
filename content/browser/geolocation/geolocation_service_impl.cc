// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_service_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

#if BUILDFLAG(IS_IOS)
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#endif

namespace content {

namespace {

using GeolocationPermissionLevel = device::mojom::GeolocationPermissionLevel;

GeolocationPermissionLevel GetPermissionLevel(
    const PermissionResult& permission_result) {
  if (base::FeatureList::IsEnabled(
          content_settings::features::kApproximateGeolocationPermission)) {
    if (permission_result.status == blink::mojom::PermissionStatus::GRANTED) {
      // A GRANTED permission must have an associated setting. The setting is
      // assumed to be the `GeolocationSetting` variant, which is then
      // extracted.
      CHECK(permission_result.retrieved_permission_setting.has_value());
      GeolocationSetting geo_setting = std::get<GeolocationSetting>(
          *(permission_result.retrieved_permission_setting));
      if (geo_setting.precise == PermissionOption::kAllowed) {
        return GeolocationPermissionLevel::kPrecise;
      } else if (geo_setting.approximate == PermissionOption::kAllowed) {
        return GeolocationPermissionLevel::kApproximate;
      }
    }
    // Otherwise, the permission is considered denied.
    return GeolocationPermissionLevel::kDenied;
  }
  // With the feature disabled, the result is either granted for precise or
  // denied.
  return permission_result.status == blink::mojom::PermissionStatus::GRANTED
             ? GeolocationPermissionLevel::kPrecise
             : GeolocationPermissionLevel::kDenied;
}
}  // namespace

GeolocationServiceImplContext::GeolocationServiceImplContext() = default;

GeolocationServiceImplContext::~GeolocationServiceImplContext() = default;

void GeolocationServiceImplContext::RequestPermission(
    RenderFrameHost* render_frame_host,
    bool user_gesture,
    PermissionCallback callback) {
  if (has_pending_permission_request_) {
    mojo::ReportBadMessage(
        "GeolocationService client may only create one Geolocation at a "
        "time.");
    return;
  }

  has_pending_permission_request_ = true;

  render_frame_host->GetBrowserContext()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          render_frame_host,
          PermissionRequestDescription(
              content::PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionType(
                      blink::PermissionType::GEOLOCATION),
              user_gesture),
          base::BindOnce(&GeolocationServiceImplContext::HandlePermissionResult,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void GeolocationServiceImplContext::HandlePermissionResult(
    PermissionCallback callback,
    PermissionResult permission_result) {
  has_pending_permission_request_ = false;
  std::move(callback).Run(permission_result);
}

GeolocationServiceImpl::GeolocationServiceImpl(
    device::mojom::GeolocationContext* geolocation_context,
    RenderFrameHost* render_frame_host)
    : geolocation_context_(geolocation_context),
      render_frame_host_(render_frame_host) {
  DCHECK(geolocation_context);
  DCHECK(render_frame_host);
}

GeolocationServiceImpl::~GeolocationServiceImpl() {
  DecrementActivityCount();
}

void GeolocationServiceImpl::Bind(
    mojo::PendingReceiver<blink::mojom::GeolocationService> receiver) {
  receiver_set_.Add(this, std::move(receiver),
                    std::make_unique<GeolocationServiceImplContext>());
  receiver_set_.set_disconnect_handler(base::BindRepeating(
      &GeolocationServiceImpl::OnDisconnected, base::Unretained(this)));
#if BUILDFLAG(IS_IOS)
  device::GeolocationSystemPermissionManager*
      geolocation_system_permission_manager =
          device::GeolocationSystemPermissionManager::GetInstance();
  if (geolocation_system_permission_manager) {
    geolocation_system_permission_manager->RequestSystemPermission();
  }
#endif
}

void GeolocationServiceImpl::CreateGeolocation(
    mojo::PendingReceiver<device::mojom::Geolocation> receiver,
    bool user_gesture,
    CreateGeolocationCallback callback) {
  if (!render_frame_host_->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kGeolocation)) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  // If the geolocation service is destroyed before the callback is run, ensure
  // it is called with DENIED status.
  auto scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), blink::mojom::PermissionStatus::DENIED);

  receiver_set_.current_context()->RequestPermission(
      render_frame_host_, user_gesture,
      // The owning RenderFrameHost might be destroyed before the permission
      // request finishes. To avoid calling a callback on a destroyed object,
      // use a WeakPtr and skip the callback if the object is invalid.
      base::BindOnce(
          &GeolocationServiceImpl::CreateGeolocationWithPermissionResult,
          weak_factory_.GetWeakPtr(), std::move(receiver),
          std::move(scoped_callback)));
}

void GeolocationServiceImpl::CreateGeolocationWithPermissionResult(
    mojo::PendingReceiver<device::mojom::Geolocation> receiver,
    CreateGeolocationCallback callback,
    PermissionResult permission_result) {
  GeolocationPermissionLevel permission_level =
      GetPermissionLevel(permission_result);
  if (permission_level == GeolocationPermissionLevel::kDenied) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  std::move(callback).Run(blink::mojom::PermissionStatus::GRANTED);
  IncrementActivityCount();

  requesting_origin_ =
      render_frame_host_->GetMainFrame()->GetLastCommittedOrigin();
  auto requesting_url =
      render_frame_host_->GetMainFrame()->GetLastCommittedURL();

  bool has_precise_permission =
      permission_level == GeolocationPermissionLevel::kPrecise;
  geolocation_context_->BindGeolocation(
      std::move(receiver), requesting_url,
      device::mojom::GeolocationClientId::kGeolocationServiceImpl,
      has_precise_permission);
  subscription_id_ =
      PermissionControllerImpl::FromBrowserContext(
          render_frame_host_->GetBrowserContext())
          ->SubscribeToPermissionResultChange(
              PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionType(
                      blink::PermissionType::GEOLOCATION),
              /*render_process_host=*/nullptr, render_frame_host_,
              requesting_url,
              /*should_include_device_status=*/false,
              base::BindRepeating(
                  &GeolocationServiceImpl::HandlePermissionResultChange,
                  weak_factory_.GetWeakPtr()));
}

void GeolocationServiceImpl::HandlePermissionResultChange(
    PermissionResult permission_result) {
  GeolocationPermissionLevel permission_level =
      GetPermissionLevel(permission_result);
  if (permission_level == GeolocationPermissionLevel::kDenied &&
      subscription_id_.value()) {
    PermissionControllerImpl::FromBrowserContext(
        render_frame_host_->GetBrowserContext())
        ->UnsubscribeFromPermissionResultChange(subscription_id_);
    DecrementActivityCount();
  }
  geolocation_context_->OnPermissionUpdated(requesting_origin_,
                                            permission_level);
}

void GeolocationServiceImpl::OnDisconnected() {
  if (receiver_set_.empty()) {
    DecrementActivityCount();
  }
}

void GeolocationServiceImpl::IncrementActivityCount() {
  is_sending_updates_ = true;
  auto* web_contents = WebContents::FromRenderFrameHost(render_frame_host_);
  static_cast<WebContentsImpl*>(web_contents)
      ->IncrementGeolocationActiveFrameCount();
}

void GeolocationServiceImpl::DecrementActivityCount() {
  if (is_sending_updates_) {
    is_sending_updates_ = false;
    auto* web_contents = WebContents::FromRenderFrameHost(render_frame_host_);
    static_cast<WebContentsImpl*>(web_contents)
        ->DecrementGeolocationActiveFrameCount();
  }
}

}  // namespace content
