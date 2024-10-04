// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_controller_impl.h"

#include "base/functional/bind.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/common/features.h"
#endif

namespace content {

namespace {

constexpr char kPermissionBlockedFencedFrameMessage[] =
    "%s permission has been blocked because it was requested inside a fenced "
    "frame. Fenced frames don't currently support permission requests.";

#if !BUILDFLAG(IS_ANDROID)
const char kPermissionBlockedPermissionsPolicyMessage[] =
    "%s permission has been blocked because of a permissions policy applied to"
    " the current document. See https://goo.gl/EuHzyv for more details.";
#endif

std::optional<blink::scheduler::WebSchedulerTrackedFeature>
PermissionToSchedulingFeature(PermissionType permission_name) {
  switch (permission_name) {
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
    // These two permissions are in the process of being split; they share logic
    // for now. TODO(crbug.com/40246640): split and consolidate as much as
    // possible.
    case PermissionType::TOP_LEVEL_STORAGE_ACCESS:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedStorageAccessGrant;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
    case PermissionType::DURABLE_STORAGE:
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
    case PermissionType::HAND_TRACKING:
    case PermissionType::CAMERA_PAN_TILT_ZOOM:
    case PermissionType::WINDOW_MANAGEMENT:
    case PermissionType::LOCAL_FONTS:
    case PermissionType::DISPLAY_CAPTURE:
    case PermissionType::GEOLOCATION:
    case PermissionType::NOTIFICATIONS:
    case PermissionType::CAPTURED_SURFACE_CONTROL:
    case PermissionType::SMART_CARD:
    case PermissionType::WEB_PRINTING:
    case PermissionType::SPEAKER_SELECTION:
    case PermissionType::KEYBOARD_LOCK:
    case PermissionType::POINTER_LOCK:
    case PermissionType::AUTOMATIC_FULLSCREEN:
    case PermissionType::WEB_APP_INSTALLATION:
      return std::nullopt;
  }
}

void LogPermissionBlockedMessage(PermissionType permission,
                                 RenderFrameHost* rfh,
                                 const char* message) {
  rfh->GetOutermostMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      base::StringPrintfNonConstexpr(
          message, blink::GetPermissionString(permission).c_str()));
}

#if !BUILDFLAG(IS_ANDROID)
bool PermissionAllowedByPermissionsPolicy(PermissionType permission_type,
                                          RenderFrameHost* rfh) {
  const auto permission_policy =
      blink::PermissionTypeToPermissionsPolicyFeature(permission_type);
  // Some features don't have an associated permissions policy yet. Allow those.
  if (!permission_policy.has_value()) {
    return true;
  }

  return rfh->IsFeatureEnabled(permission_policy.value());
}
#endif

PermissionResult VerifyContextOfCurrentDocument(
    PermissionType permission,
    RenderFrameHost* render_frame_host) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);

  DCHECK(web_contents);

  // Permissions are denied for fenced frames.
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    return PermissionResult(PermissionStatus::DENIED,
                            PermissionStatusSource::FENCED_FRAME);
  }

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kPermissionsPolicyVerificationInContent)) {
    // Check whether the feature is enabled for the frame by permissions policy.
    if (!PermissionAllowedByPermissionsPolicy(permission, render_frame_host)) {
      return PermissionResult(PermissionStatus::DENIED,
                              PermissionStatusSource::FEATURE_POLICY);
    }
  }
#endif

  return PermissionResult(PermissionStatus::ASK,
                          PermissionStatusSource::UNSPECIFIED);
}

bool IsRequestAllowed(
    const std::vector<blink::PermissionType>& permissions,
    RenderFrameHost* render_frame_host,
    base::OnceCallback<void(const std::vector<PermissionStatus>&)>& callback) {
  if (!render_frame_host) {
    // Permission request is not allowed without a valid RenderFrameHost.
    std::move(callback).Run(std::vector<PermissionStatus>(
        permissions.size(), PermissionStatus::ASK));
    return false;
  }

  // Verifies and evicts `render_frame_host` from BFcache. Returns true if
  // render_frame_host was evicted, returns false otherwise.
  if (render_frame_host->IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kRequestPermission)) {
    std::move(callback).Run(std::vector<PermissionStatus>(
        permissions.size(), PermissionStatus::ASK));
    return false;
  }

  // Verify each permission independently to generate proper warning messages.
  bool is_permission_allowed = true;
  for (PermissionType permission : permissions) {
    PermissionResult result =
        VerifyContextOfCurrentDocument(permission, render_frame_host);

    if (result.status == PermissionStatus::DENIED) {
      switch (result.source) {
        case PermissionStatusSource::FENCED_FRAME:
          LogPermissionBlockedMessage(permission, render_frame_host,
                                      kPermissionBlockedFencedFrameMessage);
          break;
#if !BUILDFLAG(IS_ANDROID)
        case PermissionStatusSource::FEATURE_POLICY:
          LogPermissionBlockedMessage(
              permission, render_frame_host,
              kPermissionBlockedPermissionsPolicyMessage);
          break;
#endif
        default:
          break;
      }

      is_permission_allowed = false;
    }
  }

  if (!is_permission_allowed) {
    std::move(callback).Run(std::vector<PermissionStatus>(
        permissions.size(), PermissionStatus::DENIED));
    return false;
  }

  return true;
}

void NotifySchedulerAboutPermissionRequest(RenderFrameHost* render_frame_host,
                                           PermissionType permission_name) {
  DCHECK(render_frame_host);

  std::optional<blink::scheduler::WebSchedulerTrackedFeature> feature =
      PermissionToSchedulingFeature(permission_name);

  if (!feature) {
    return;
  }

  static_cast<RenderFrameHostImpl*>(render_frame_host)
      ->OnBackForwardCacheDisablingStickyFeatureUsed(feature.value());
}

// Calls |original_cb|, a callback expecting the PermissionStatus of a set of
// permissions, after joining the results of overridden permissions and
// non-overridden permissions.
// |overridden_results| is an array of permissions that have already been
// overridden by DevTools.
// |delegated_results| contains results that did not have overrides - they
// were delegated - their results need to be inserted in order.
void MergeOverriddenAndDelegatedResults(
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> original_cb,
    std::vector<std::optional<PermissionStatus>> overridden_results,
    const std::vector<PermissionStatus>& delegated_results) {
  std::vector<PermissionStatus> full_results;
  full_results.reserve(overridden_results.size());
  auto delegated_it = delegated_results.begin();
  for (auto& status : overridden_results) {
    if (!status) {
      CHECK(delegated_it != delegated_results.end());
      status.emplace(*delegated_it++);
    }
    full_results.emplace_back(*status);
  }
  CHECK(delegated_it == delegated_results.end());

  std::move(original_cb).Run(full_results);
}

void PermissionStatusCallbackWrapper(
    base::OnceCallback<void(PermissionStatus)> callback,
    const std::vector<PermissionStatus>& vector) {
  DCHECK_EQ(1ul, vector.size());
  std::move(callback).Run(vector.at(0));
}

// Removes from |description.permissions| the entries that have an override
// status (as per the provided overrides). Returns a result vector that contains
// all the statuses for permissions after applying overrides, using `nullopt`
// for those permissions that do not have an override.
std::vector<std::optional<blink::mojom::PermissionStatus>> OverridePermissions(
    PermissionRequestDescription& description,
    RenderFrameHost* render_frame_host,
    const PermissionOverrides& permission_overrides) {
  std::vector<blink::PermissionType> permissions_without_overrides;
  std::vector<std::optional<blink::mojom::PermissionStatus>> results;
  const url::Origin& origin = render_frame_host->GetLastCommittedOrigin();
  for (const auto& permission : description.permissions) {
    std::optional<blink::mojom::PermissionStatus> override_status =
        permission_overrides.Get(origin, permission);
    if (!override_status) {
      permissions_without_overrides.push_back(permission);
    }
    results.push_back(override_status);
  }

  description.permissions = std::move(permissions_without_overrides);
  return results;
}

}  // namespace

PermissionControllerImpl::PermissionControllerImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

// static
PermissionControllerImpl* PermissionControllerImpl::FromBrowserContext(
    BrowserContext* browser_context) {
  return static_cast<PermissionControllerImpl*>(
      browser_context->GetPermissionController());
}

PermissionControllerImpl::~PermissionControllerImpl() {
  // Ideally we need to unsubscribe from delegate subscriptions here,
  // but browser_context_ is already destroyed by this point, so
  // we can't fetch our delegate.
}

PermissionStatus PermissionControllerImpl::GetSubscriptionCurrentValue(
    const PermissionStatusSubscription& subscription) {
  // The RFH may be null if the request is for a worker.
  RenderFrameHost* rfh = RenderFrameHost::FromID(subscription.render_process_id,
                                                 subscription.render_frame_id);
  if (rfh) {
    return GetPermissionStatusForCurrentDocument(subscription.permission, rfh);
  }

  RenderProcessHost* rph =
      RenderProcessHost::FromID(subscription.render_process_id);
  if (rph) {
    return GetPermissionStatusForWorker(
        subscription.permission, rph,
        url::Origin::Create(subscription.requesting_origin));
  }

  return GetPermissionStatusInternal(subscription.permission,
                                     subscription.requesting_origin,
                                     subscription.embedding_origin);
}

PermissionControllerImpl::SubscriptionsStatusMap
PermissionControllerImpl::GetSubscriptionsStatuses(
    const std::optional<GURL>& origin) {
  SubscriptionsStatusMap statuses;
  for (SubscriptionsMap::iterator iter(&subscriptions_); !iter.IsAtEnd();
       iter.Advance()) {
    PermissionStatusSubscription* subscription = iter.GetCurrentValue();
    if (origin.has_value() && subscription->requesting_origin != *origin) {
      continue;
    }
    statuses[iter.GetCurrentKey()] = GetSubscriptionCurrentValue(*subscription);
  }
  return statuses;
}

void PermissionControllerImpl::NotifyChangedSubscriptions(
    const SubscriptionsStatusMap& old_statuses) {
  std::vector<base::OnceClosure> callbacks;
  for (const auto& it : old_statuses) {
    auto key = it.first;
    PermissionStatusSubscription* subscription = subscriptions_.Lookup(key);
    if (!subscription) {
      continue;
    }
    PermissionStatus old_status = it.second;
    PermissionStatus new_status = GetSubscriptionCurrentValue(*subscription);
    if (new_status != old_status) {
      // This is a private method that is called internally if a permission
      // status was set by DevTools. Suppress permission status override
      // verification and always notify listeners.
      callbacks.push_back(base::BindOnce(subscription->callback, new_status,
                                         /*ignore_status_override=*/true));
    }
  }
  for (auto& callback : callbacks)
    std::move(callback).Run();
}

PermissionControllerImpl::OverrideStatus
PermissionControllerImpl::GrantOverridesForDevTools(
    const std::optional<url::Origin>& origin,
    const std::vector<PermissionType>& permissions) {
  return GrantPermissionOverrides(origin, permissions);
}

PermissionControllerImpl::OverrideStatus
PermissionControllerImpl::SetOverrideForDevTools(
    const std::optional<url::Origin>& origin,
    PermissionType permission,
    const PermissionStatus& status) {
  return SetPermissionOverride(origin, permission, status);
}

void PermissionControllerImpl::ResetOverridesForDevTools() {
  ResetPermissionOverrides();
}

PermissionControllerImpl::OverrideStatus
PermissionControllerImpl::SetPermissionOverride(
    const std::optional<url::Origin>& origin,
    PermissionType permission,
    const PermissionStatus& status) {
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate && !delegate->IsPermissionOverridable(permission, origin)) {
    return OverrideStatus::kOverrideNotSet;
  }
  const auto old_statuses = GetSubscriptionsStatuses(
      origin ? std::make_optional(origin->GetURL()) : std::nullopt);
  permission_overrides_.Set(origin, permission, status);
  NotifyChangedSubscriptions(old_statuses);

  return OverrideStatus::kOverrideSet;
}

PermissionControllerImpl::OverrideStatus
PermissionControllerImpl::GrantPermissionOverrides(
    const std::optional<url::Origin>& origin,
    const std::vector<PermissionType>& permissions) {
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate) {
    for (const auto permission : permissions) {
      if (!delegate->IsPermissionOverridable(permission, origin)) {
        return OverrideStatus::kOverrideNotSet;
      }
    }
  }

  const auto old_statuses = GetSubscriptionsStatuses(
      origin ? std::make_optional(origin->GetURL()) : std::nullopt);
  permission_overrides_.GrantPermissions(origin, permissions);
  // If any statuses changed because they lose overrides or the new overrides
  // modify their previous state (overridden or not), subscribers must be
  // notified manually.
  NotifyChangedSubscriptions(old_statuses);

  return OverrideStatus::kOverrideSet;
}

void PermissionControllerImpl::ResetPermissionOverrides() {
  const auto old_statuses = GetSubscriptionsStatuses();
  permission_overrides_ = PermissionOverrides();

  // If any statuses changed because they lost their overrides, the subscribers
  // must be notified manually.
  NotifyChangedSubscriptions(old_statuses);
}

void PermissionControllerImpl::RequestPermissions(
    RenderFrameHost* render_frame_host,
    PermissionRequestDescription request_description,
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  if (!IsRequestAllowed(request_description.permissions, render_frame_host,
                        callback)) {
    return;
  }

  for (PermissionType permission : request_description.permissions) {
    NotifySchedulerAboutPermissionRequest(render_frame_host, permission);
  }

  std::vector<std::optional<blink::mojom::PermissionStatus>> override_results =
      OverridePermissions(request_description, render_frame_host,
                          permission_overrides_);

  auto wrapper = base::BindOnce(&MergeOverriddenAndDelegatedResults,
                                std::move(callback), override_results);
  if (request_description.permissions.empty()) {
    std::move(wrapper).Run({});
    return;
  }

  // Use delegate to find statuses of other permissions that have been requested
  // but do not have overrides.
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    std::move(wrapper).Run(std::vector<PermissionStatus>(
        request_description.permissions.size(), PermissionStatus::DENIED));
    return;
  }

  delegate->RequestPermissions(render_frame_host, request_description,
                               std::move(wrapper));
}

void PermissionControllerImpl::RequestPermissionFromCurrentDocument(
    RenderFrameHost* render_frame_host,
    PermissionRequestDescription request_description,
    base::OnceCallback<void(PermissionStatus)> callback) {
  RequestPermissionsFromCurrentDocument(
      render_frame_host, std::move(request_description),
      base::BindOnce(&PermissionStatusCallbackWrapper, std::move(callback)));
}

void PermissionControllerImpl::RequestPermissionsFromCurrentDocument(
    RenderFrameHost* render_frame_host,
    PermissionRequestDescription request_description,
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  if (!IsRequestAllowed(request_description.permissions, render_frame_host,
                        callback)) {
    return;
  }

  for (PermissionType permission : request_description.permissions) {
    NotifySchedulerAboutPermissionRequest(render_frame_host, permission);
  }

  request_description.requesting_origin =
      render_frame_host->GetLastCommittedOrigin().GetURL();
  std::vector<std::optional<blink::mojom::PermissionStatus>> override_results =
      OverridePermissions(request_description, render_frame_host,
                          permission_overrides_);

  auto wrapper = base::BindOnce(&MergeOverriddenAndDelegatedResults,
                                std::move(callback), override_results);
  if (request_description.permissions.empty()) {
    std::move(wrapper).Run({});
    return;
  }

  // Use delegate to find statuses of other permissions that have been requested
  // but do not have overrides.
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    std::move(wrapper).Run(std::vector<PermissionStatus>(
        request_description.permissions.size(), PermissionStatus::DENIED));
    return;
  }

  delegate->RequestPermissionsFromCurrentDocument(
      render_frame_host, request_description, std::move(wrapper));
}

void PermissionControllerImpl::ResetPermission(blink::PermissionType permission,
                                               const url::Origin& origin) {
  ResetPermission(permission, origin.GetURL(), origin.GetURL());
}

PermissionStatus PermissionControllerImpl::GetPermissionStatusInternal(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  std::optional<PermissionStatus> status = permission_overrides_.Get(
      url::Origin::Create(requesting_origin), permission);
  if (status) {
    return *status;
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionStatus::DENIED;
  }

  return delegate->GetPermissionStatus(permission, requesting_origin,
                                       embedding_origin);
}

PermissionStatus
PermissionControllerImpl::GetPermissionStatusForCurrentDocumentInternal(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  std::optional<PermissionStatus> status = permission_overrides_.Get(
      render_frame_host->GetLastCommittedOrigin(), permission);
  if (status) {
    return *status;
  }
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionStatus::DENIED;
  }
  if (VerifyContextOfCurrentDocument(permission, render_frame_host).status ==
      PermissionStatus::DENIED) {
    return PermissionStatus::DENIED;
  }
  return delegate->GetPermissionStatusForCurrentDocument(
      permission, render_frame_host, should_include_device_status);
}

PermissionStatus PermissionControllerImpl::GetPermissionStatusForWorker(
    PermissionType permission,
    RenderProcessHost* render_process_host,
    const url::Origin& worker_origin) {
  std::optional<PermissionStatus> status =
      permission_overrides_.Get(worker_origin, permission);
  if (status.has_value()) {
    return *status;
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionStatus::DENIED;
  }

  return delegate->GetPermissionStatusForWorker(permission, render_process_host,
                                                worker_origin.GetURL());
}

PermissionStatus
PermissionControllerImpl::GetPermissionStatusForCurrentDocument(
    PermissionType permission,
    RenderFrameHost* render_frame_host) {
  return GetPermissionStatusForCurrentDocumentInternal(permission,
                                                       render_frame_host);
}

PermissionResult
PermissionControllerImpl::GetPermissionResultForCurrentDocument(
    PermissionType permission,
    RenderFrameHost* render_frame_host) {
  std::optional<PermissionStatus> status = permission_overrides_.Get(
      render_frame_host->GetLastCommittedOrigin(), permission);
  if (status) {
    return PermissionResult(*status, PermissionStatusSource::UNSPECIFIED);
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionResult(PermissionStatus::DENIED,
                            PermissionStatusSource::UNSPECIFIED);
  }

  PermissionResult result =
      VerifyContextOfCurrentDocument(permission, render_frame_host);
  if (result.status == PermissionStatus::DENIED) {
    return result;
  }

  return delegate->GetPermissionResultForCurrentDocument(
      permission, render_frame_host, /*should_include_device_status=*/false);
}

PermissionResult
PermissionControllerImpl::GetPermissionResultForOriginWithoutContext(
    PermissionType permission,
    const url::Origin& origin) {
  return GetPermissionResultForOriginWithoutContext(permission, origin, origin);
}

PermissionResult
PermissionControllerImpl::GetPermissionResultForOriginWithoutContext(
    PermissionType permission,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  std::optional<PermissionStatus> status =
      permission_overrides_.Get(requesting_origin, permission);
  if (status) {
    return PermissionResult(*status, PermissionStatusSource::UNSPECIFIED);
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionResult(PermissionStatus::DENIED,
                            PermissionStatusSource::UNSPECIFIED);
  }

  return delegate->GetPermissionResultForOriginWithoutContext(
      permission, requesting_origin, embedding_origin);
}

PermissionStatus
PermissionControllerImpl::GetPermissionStatusForEmbeddedRequester(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host,
    const url::Origin& requesting_origin) {
  // This API is suited only for `TOP_LEVEL_STORAGE_ACCESS`. Do not use it for
  // other permissions unless discussed with `permissions-core@`.
  DCHECK(permission == blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS);

  if (permission != blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS) {
    return PermissionStatus::DENIED;
  }

  std::optional<PermissionStatus> status =
      permission_overrides_.Get(requesting_origin, permission);
  if (status) {
    return *status;
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionStatus::DENIED;
  }

  if (VerifyContextOfCurrentDocument(permission, render_frame_host).status ==
      PermissionStatus::DENIED) {
    return PermissionStatus::DENIED;
  }

  return delegate->GetPermissionStatusForEmbeddedRequester(
      permission, render_frame_host, requesting_origin);
}

PermissionStatus PermissionControllerImpl::GetCombinedPermissionAndDeviceStatus(
    PermissionType permission,
    RenderFrameHost* render_frame_host) {
  CHECK(permission == blink::PermissionType::VIDEO_CAPTURE ||
        permission == blink::PermissionType::AUDIO_CAPTURE ||
        permission == blink::PermissionType::GEOLOCATION);
  return GetPermissionStatusForCurrentDocumentInternal(
      permission, render_frame_host, /*should_include_device_status=*/true);
}

void PermissionControllerImpl::ResetPermission(PermissionType permission,
                                               const GURL& requesting_origin,
                                               const GURL& embedding_origin) {
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return;
  }
  delegate->ResetPermission(permission, requesting_origin, embedding_origin);
}

void PermissionControllerImpl::PermissionStatusChange(
    const base::RepeatingCallback<void(PermissionStatus)>& callback,
    SubscriptionId subscription_id,
    PermissionStatus status,
    bool ignore_status_override) {
  // Check if the permission status override should be ignored. The verification
  // is suppressed if a permission status change was initiated by DevTools. In
  // all other cases permission status override is always checked.
  if (ignore_status_override) {
    callback.Run(status);
    return;
  }
  PermissionStatusSubscription* subscription =
      subscriptions_.Lookup(subscription_id);
  DCHECK(subscription);
  // TODO(crbug.com/40056329) Adding this block to prevent crashes while we
  // investigate the root cause of the crash. This block will be removed as the
  // DCHECK() above should be enough.
  if (!subscription) {
    return;
  }
  std::optional<PermissionStatus> status_override = permission_overrides_.Get(
      url::Origin::Create(subscription->requesting_origin),
      subscription->permission);
  if (!status_override.has_value()) {
    callback.Run(status);
  }
}

PermissionController::SubscriptionId
PermissionControllerImpl::SubscribeToPermissionStatusChange(
    PermissionType permission,
    RenderProcessHost* render_process_host,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool should_include_device_status,
    const base::RepeatingCallback<void(PermissionStatus)>& callback) {
  DCHECK(!render_process_host || !render_frame_host);

  auto id = subscription_id_generator_.GenerateNextId();
  auto subscription = std::make_unique<PermissionStatusSubscription>();
  subscription->permission = permission;
  subscription->callback =
      base::BindRepeating(&PermissionControllerImpl::PermissionStatusChange,
                          base::Unretained(this), callback, id);
  subscription->requesting_origin = requesting_origin;
  subscription->should_include_device_status = should_include_device_status;

  // The RFH may be null if the request is for a worker.
  if (render_frame_host) {
    subscription->embedding_origin =
        PermissionUtil::GetLastCommittedOriginAsURL(
            render_frame_host->GetMainFrame());
    subscription->render_frame_id = render_frame_host->GetRoutingID();
    subscription->render_process_id = render_frame_host->GetProcess()->GetID();
  } else {
    subscription->embedding_origin = requesting_origin;
    subscription->render_frame_id = -1;
    subscription->render_process_id =
        render_process_host ? render_process_host->GetID() : -1;
  }
  subscriptions_.AddWithID(std::move(subscription), id);

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate) {
    delegate->SetSubscriptions(&subscriptions_);
    delegate->OnPermissionStatusChangeSubscriptionAdded(id);
  }
  return id;
}

void PermissionControllerImpl::UnsubscribeFromPermissionStatusChange(
    SubscriptionId subscription_id) {
  PermissionStatusSubscription* subscription =
      subscriptions_.Lookup(subscription_id);
  if (!subscription) {
    return;
  }
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate) {
    delegate->UnsubscribeFromPermissionStatusChange(subscription_id);
  }
  subscriptions_.Remove(subscription_id);
}

bool PermissionControllerImpl::IsSubscribedToPermissionChangeEvent(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host) {
  PermissionServiceContext* permission_service_context =
      PermissionServiceContext::GetForCurrentDocument(render_frame_host);
  if (!permission_service_context) {
    return false;
  }

  return permission_service_context->GetOnchangeEventListeners().find(
             permission) !=
         permission_service_context->GetOnchangeEventListeners().end();
}

std::optional<gfx::Rect>
PermissionControllerImpl::GetExclusionAreaBoundsInScreen(
    WebContents* web_contents) const {
  if (exclusion_area_bounds_for_tests_.has_value()) {
    return exclusion_area_bounds_for_tests_;
  }
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  return delegate ? delegate->GetExclusionAreaBoundsInScreen(web_contents)
                  : std::nullopt;
}

void PermissionControllerImpl::NotifyEventListener() {
  if (onchange_listeners_callback_for_tests_) {
    onchange_listeners_callback_for_tests_.Run();
  }
}

}  // namespace content
