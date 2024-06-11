// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

#if BUILDFLAG(IS_IOS)
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#endif

namespace content {

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
          PermissionRequestDescription(blink::PermissionType::GEOLOCATION,
                                       user_gesture),
          base::BindOnce(&GeolocationServiceImplContext::HandlePermissionStatus,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void GeolocationServiceImplContext::HandlePermissionStatus(
    PermissionCallback callback,
    blink::mojom::PermissionStatus permission_status) {
  has_pending_permission_request_ = false;
  std::move(callback).Run(permission_status);
}

GeolocationServiceImpl::GeolocationServiceImpl(
    device::mojom::GeolocationContext* geolocation_context,
    RenderFrameHost* render_frame_host)
    : geolocation_context_(geolocation_context),
      render_frame_host_(render_frame_host) {
  DCHECK(geolocation_context);
  DCHECK(render_frame_host);
}

GeolocationServiceImpl::~GeolocationServiceImpl() = default;

void GeolocationServiceImpl::Bind(
    mojo::PendingReceiver<blink::mojom::GeolocationService> receiver) {
  receiver_set_.Add(this, std::move(receiver),
                    std::make_unique<GeolocationServiceImplContext>());
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
          blink::mojom::PermissionsPolicyFeature::kGeolocation)) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  // If the geolocation service is destroyed before the callback is run, ensure
  // it is called with DENIED status.
  auto scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), blink::mojom::PermissionStatus::DENIED);

  receiver_set_.current_context()->RequestPermission(
      render_frame_host_, user_gesture,
      // There is an assumption here that the GeolocationServiceImplContext will
      // outlive the GeolocationServiceImpl.
      base::BindOnce(
          &GeolocationServiceImpl::CreateGeolocationWithPermissionStatus,
          base::Unretained(this), std::move(receiver),
          std::move(scoped_callback)));
}

void GeolocationServiceImpl::CreateGeolocationWithPermissionStatus(
    mojo::PendingReceiver<device::mojom::Geolocation> receiver,
    CreateGeolocationCallback callback,
    blink::mojom::PermissionStatus permission_status) {
  std::move(callback).Run(permission_status);
  if (permission_status != blink::mojom::PermissionStatus::GRANTED)
    return;

  requesting_origin_ =
      render_frame_host_->GetMainFrame()->GetLastCommittedOrigin();
  auto requesting_url =
      render_frame_host_->GetMainFrame()->GetLastCommittedURL();

  geolocation_context_->BindGeolocation(
      std::move(receiver), requesting_url,
      device::mojom::GeolocationClientId::kGeolocationServiceImpl);
  subscription_id_ =
      PermissionControllerImpl::FromBrowserContext(
          render_frame_host_->GetBrowserContext())
          ->SubscribeToPermissionStatusChange(
              blink::PermissionType::GEOLOCATION,
              /*render_process_host=*/nullptr, render_frame_host_,
              requesting_url,
              /*should_include_device_status=*/false,
              base::BindRepeating(
                  &GeolocationServiceImpl::HandlePermissionStatusChange,
                  weak_factory_.GetWeakPtr()));
}

void GeolocationServiceImpl::HandlePermissionStatusChange(
    blink::mojom::PermissionStatus permission_status) {
  if (permission_status != blink::mojom::PermissionStatus::GRANTED &&
      subscription_id_.value()) {
    PermissionControllerImpl::FromBrowserContext(
        render_frame_host_->GetBrowserContext())
        ->UnsubscribeFromPermissionStatusChange(subscription_id_);
    geolocation_context_->OnPermissionRevoked(requesting_origin_);
  }
}

}  // namespace content
