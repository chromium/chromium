// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_CC_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_CC_

#include "content/browser/permissions/permission_controller_impl.h"
#include "base/bind.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class GURL;

namespace content {

namespace {

base::Optional<blink::scheduler::WebSchedulerTrackedFeature>
PermissionToSchedulingFeature(PermissionType permission_name) {
  switch (permission_name) {
    case PermissionType::GEOLOCATION:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedGeolocationPermission;
    case PermissionType::NOTIFICATIONS:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedNotificationsPermission;
    case PermissionType::MIDI:
    case PermissionType::MIDI_SYSEX:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedMIDIPermission;
    case PermissionType::AUDIO_CAPTURE:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedAudioCapturePermission;
    case PermissionType::VIDEO_CAPTURE:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedVideoCapturePermission;
    case PermissionType::BACKGROUND_SYNC:
    case PermissionType::BACKGROUND_FETCH:
    case PermissionType::PERIODIC_BACKGROUND_SYNC:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedBackgroundWorkPermission;
    case PermissionType::STORAGE_ACCESS_GRANT:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedStorageAccessGrant;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
    case PermissionType::DURABLE_STORAGE:
    case PermissionType::FLASH:
    case PermissionType::ACCESSIBILITY_EVENTS:
    case PermissionType::CLIPBOARD_READ_WRITE:
    case PermissionType::CLIPBOARD_SANITIZED_WRITE:
    case PermissionType::PAYMENT_HANDLER:
    case PermissionType::IDLE_DETECTION:
    case PermissionType::WAKE_LOCK_SCREEN:
    case PermissionType::WAKE_LOCK_SYSTEM:
    case PermissionType::NFC:
    case PermissionType::NUM:
    case PermissionType::SENSORS:
    case PermissionType::AR:
    case PermissionType::VR:
    case PermissionType::CAMERA_PAN_TILT_ZOOM:
    case PermissionType::WINDOW_PLACEMENT:
    case PermissionType::FONT_ACCESS:
      return base::nullopt;
  }
}

void NotifySchedulerAboutPermissionRequest(RenderFrameHost* render_frame_host,
                                           PermissionType permission_name) {
  DCHECK(render_frame_host);

  base::Optional<blink::scheduler::WebSchedulerTrackedFeature> feature =
      PermissionToSchedulingFeature(permission_name);

  if (!feature)
    return;

  static_cast<RenderFrameHostImpl*>(render_frame_host)
      ->OnSchedulerTrackedFeatureUsed(feature.value());
}

// Calls |original_cb|, a callback expecting the PermissionStatus of a set of
// permissions, after joining the results of overridden permissions and
// non-overridden permissions.
// |overridden_results| is an array of permissions that have already been
// overridden by DevTools.
// |delegated_results| contains results that did not have overrides - they
// were delegated - their results need to be inserted in order.
void MergeOverriddenAndDelegatedResults(
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        original_cb,
    std::vector<base::Optional<blink::mojom::PermissionStatus>>
        overridden_results,
    const std::vector<blink::mojom::PermissionStatus>& delegated_results) {
  std::vector<blink::mojom::PermissionStatus> full_results;
  full_results.reserve(overridden_results.size());
  auto delegated_it = delegated_results.begin();
  for (auto& status : overridden_results) {
    if (!status.has_value()) {
      CHECK(delegated_it != delegated_results.end());
      status.emplace(*delegated_it++);
    }
    full_results.emplace_back(*status);
  }
  CHECK(delegated_it == delegated_results.end());

  std::move(original_cb).Run(full_results);
}

}  // namespace

PermissionControllerImpl::PermissionControllerImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

// static
PermissionControllerImpl* PermissionControllerImpl::FromBrowserContext(
    BrowserContext* browser_context) {
  return static_cast<PermissionControllerImpl*>(
      BrowserContext::GetPermissionController(browser_context));
}

struct PermissionControllerImpl::Subscription {
  PermissionType permission;
  GURL requesting_origin;
  GURL embedding_origin;
  int render_frame_id = -1;
  int render_process_id = -1;
  base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback;
  int delegate_subscription_id;
};

PermissionControllerImpl::~PermissionControllerImpl() {
  // Ideally we need to unsubscribe from delegate subscriptions here,
  // but browser_context_ is already destroyed by this point, so
  // we can't fetch our delegate.
}

blink::mojom::PermissionStatus
PermissionControllerImpl::GetSubscriptionCurrentValue(
    const Subscription& subscription) {
  // The RFH may be null if the request is for a worker.
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      subscription.render_process_id, subscription.render_frame_id);
  if (rfh) {
    return GetPermissionStatusForFrame(subscription.permission, rfh,
                                       subscription.requesting_origin);
  }
  return GetPermissionStatus(subscription.permission,
                             subscription.requesting_origin,
                             subscription.embedding_origin);
}

PermissionControllerImpl::SubscriptionsStatusMap
PermissionControllerImpl::GetSubscriptionsStatuses(
    const base::Optional<GURL>& origin) {
  SubscriptionsStatusMap statuses;
  for (SubscriptionsMap::iterator iter(&subscriptions_); !iter.IsAtEnd();
       iter.Advance()) {
    Subscription* subscription = iter.GetCurrentValue();
    if (origin.has_value() && subscription->requesting_origin != *origin)
      continue;
    statuses[iter.GetCurrentKey()] = GetSubscriptionCurrentValue(*subscription);
  }
  return statuses;
}

void PermissionControllerImpl::NotifyChangedSubscriptions(
    const SubscriptionsStatusMap& old_statuses) {
  std::vector<base::OnceClosure> callbacks;
  for (const auto& it : old_statuses) {
    auto key = it.first;
    Subscription* subscription = subscriptions_.Lookup(key);
    if (!subscription)
      continue;
    blink::mojom::PermissionStatus old_status = it.second;
    blink::mojom::PermissionStatus new_status =
        GetSubscriptionCurrentValue(*subscription);
    if (new_status != old_status)
      callbacks.push_back(base::BindOnce(subscription->callback, new_status));
  }
  for (auto& callback : callbacks)
    std::move(callback).Run();
}

PermissionControllerImpl::OverrideStatus
PermissionControllerImpl::SetOverrideForDevTools(
    const base::Optional<url::Origin>& origin,
    PermissionType permission,
    const blink::mojom::PermissionStatus& status) {
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate &&
      !delegate->IsPermissionOverridableByDevTools(permission, origin)) {
    return OverrideStatus::kOverrideNotSet;
  }
  const auto old_statuses = GetSubscriptionsStatuses(
      origin ? base::make_optional(origin->GetURL()) : base::nullopt);
  devtools_permission_overrides_.Set(origin, permission, status);
  NotifyChangedSubscriptions(old_statuses);

  UpdateDelegateOverridesForDevTools(origin);
  return OverrideStatus::kOverrideSet;
}

PermissionControllerImpl::OverrideStatus
PermissionControllerImpl::GrantOverridesForDevTools(
    const base::Optional<url::Origin>& origin,
    const std::vector<PermissionType>& permissions) {
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate) {
    for (const auto permission : permissions) {
      if (!delegate->IsPermissionOverridableByDevTools(permission, origin))
        return OverrideStatus::kOverrideNotSet;
    }
  }

  const auto old_statuses = GetSubscriptionsStatuses(
      origin ? base::make_optional(origin->GetURL()) : base::nullopt);
  devtools_permission_overrides_.GrantPermissions(origin, permissions);
  // If any statuses changed because they lose overrides or the new overrides
  // modify their previous state (overridden or not), subscribers must be
  // notified manually.
  NotifyChangedSubscriptions(old_statuses);

  UpdateDelegateOverridesForDevTools(origin);
  return OverrideStatus::kOverrideSet;
}

void PermissionControllerImpl::ResetOverridesForDevTools() {
  const auto old_statuses = GetSubscriptionsStatuses();
  devtools_permission_overrides_ = DevToolsPermissionOverrides();

  // If any statuses changed because they lost their overrides, the subscribers
  // must be notified manually.
  NotifyChangedSubscriptions(old_statuses);

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate)
    delegate->ResetPermissionOverridesForDevTools();
}

void PermissionControllerImpl::UpdateDelegateOverridesForDevTools(
    const base::Optional<url::Origin>& origin) {
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate)
    return;

  // If no overrides exist, still want to update with "blank" overrides.
  PermissionOverrides current_overrides =
      devtools_permission_overrides_.GetAll(origin);
  delegate->SetPermissionOverridesForDevTools(origin, current_overrides);
}

int PermissionControllerImpl::RequestPermission(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  NotifySchedulerAboutPermissionRequest(render_frame_host, permission);

  base::Optional<blink::mojom::PermissionStatus> status_override =
      devtools_permission_overrides_.Get(url::Origin::Create(requesting_origin),
                                         permission);
  if (status_override.has_value()) {
    std::move(callback).Run(*status_override);
    return kNoPendingOperation;
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return kNoPendingOperation;
  }
  return delegate->RequestPermission(permission, render_frame_host,
                                     requesting_origin, user_gesture,
                                     std::move(callback));
}

int PermissionControllerImpl::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  for (PermissionType permission : permissions)
    NotifySchedulerAboutPermissionRequest(render_frame_host, permission);

  std::vector<PermissionType> permissions_without_overrides;
  std::vector<base::Optional<blink::mojom::PermissionStatus>> results;
  url::Origin origin = url::Origin::Create(requesting_origin);
  for (const auto& permission : permissions) {
    base::Optional<blink::mojom::PermissionStatus> override_status =
        devtools_permission_overrides_.Get(origin, permission);
    if (!override_status)
      permissions_without_overrides.push_back(permission);
    results.push_back(override_status);
  }

  auto wrapper = base::BindOnce(&MergeOverriddenAndDelegatedResults,
                                std::move(callback), results);
  if (permissions_without_overrides.empty()) {
    std::move(wrapper).Run({});
    return kNoPendingOperation;
  }

  // Use delegate to find statuses of other permissions that have been requested
  // but do not have overrides.
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    std::move(wrapper).Run(std::vector<blink::mojom::PermissionStatus>(
        permissions_without_overrides.size(),
        blink::mojom::PermissionStatus::DENIED));
    return kNoPendingOperation;
  }
  return delegate->RequestPermissions(permissions_without_overrides,
                                      render_frame_host, requesting_origin,
                                      user_gesture, std::move(wrapper));
}

blink::mojom::PermissionStatus PermissionControllerImpl::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  base::Optional<blink::mojom::PermissionStatus> status =
      devtools_permission_overrides_.Get(url::Origin::Create(requesting_origin),
                                         permission);
  if (status.has_value())
    return *status;

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate)
    return blink::mojom::PermissionStatus::DENIED;
  return delegate->GetPermissionStatus(permission, requesting_origin,
                                       embedding_origin);
}

blink::mojom::PermissionStatus
PermissionControllerImpl::GetPermissionStatusForFrame(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  base::Optional<blink::mojom::PermissionStatus> status =
      devtools_permission_overrides_.Get(url::Origin::Create(requesting_origin),
                                         permission);
  if (status.has_value())
    return *status;

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate)
    return blink::mojom::PermissionStatus::DENIED;
  return delegate->GetPermissionStatusForFrame(permission, render_frame_host,
                                               requesting_origin);
}

void PermissionControllerImpl::ResetPermission(PermissionType permission,
                                               const GURL& requesting_origin,
                                               const GURL& embedding_origin) {
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate)
    return;
  delegate->ResetPermission(permission, requesting_origin, embedding_origin);
}

void PermissionControllerImpl::OnDelegatePermissionStatusChange(
    Subscription* subscription,
    blink::mojom::PermissionStatus status) {
  base::Optional<blink::mojom::PermissionStatus> status_override =
      devtools_permission_overrides_.Get(
          url::Origin::Create(subscription->requesting_origin),
          subscription->permission);
  if (!status_override.has_value())
    subscription->callback.Run(status);
}

int PermissionControllerImpl::SubscribePermissionStatusChange(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const base::RepeatingCallback<void(blink::mojom::PermissionStatus)>&
        callback) {
  auto subscription = std::make_unique<Subscription>();
  subscription->permission = permission;
  subscription->callback = callback;
  subscription->requesting_origin = requesting_origin;

  // The RFH may be null if the request is for a worker.
  if (render_frame_host) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    subscription->embedding_origin =
        web_contents->GetLastCommittedURL().GetOrigin();
    subscription->render_frame_id = render_frame_host->GetRoutingID();
    subscription->render_process_id = render_frame_host->GetProcess()->GetID();
  } else {
    subscription->embedding_origin = requesting_origin;
    subscription->render_frame_id = -1;
    subscription->render_process_id = -1;
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate) {
    subscription->delegate_subscription_id =
        delegate->SubscribePermissionStatusChange(
            permission, render_frame_host, requesting_origin,
            base::BindRepeating(
                &PermissionControllerImpl::OnDelegatePermissionStatusChange,
                base::Unretained(this), subscription.get()));
  } else {
    subscription->delegate_subscription_id = kNoPendingOperation;
  }
  return subscriptions_.Add(std::move(subscription));
}

void PermissionControllerImpl::UnsubscribePermissionStatusChange(
    int subscription_id) {
  Subscription* subscription = subscriptions_.Lookup(subscription_id);
  if (!subscription)
    return;
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate &&
      subscription->delegate_subscription_id != kNoPendingOperation) {
    delegate->UnsubscribePermissionStatusChange(
        subscription->delegate_subscription_id);
  }
  subscriptions_.Remove(subscription_id);
}

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_CC_
