// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_controller_impl.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_descriptor_util.h"
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
    case PermissionType::LOCAL_NETWORK_ACCESS:
      return std::nullopt;
  }
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

  return PermissionResult(PermissionStatus::ASK);
}

bool IsRequestAllowed(
    const std::vector<blink::mojom::PermissionDescriptorPtr>& permissions,
    RenderFrameHost* render_frame_host,
    base::OnceCallback<void(const std::vector<PermissionResult>&)>& callback) {
  if (!render_frame_host) {
    // Permission request is not allowed without a valid RenderFrameHost.
    std::move(callback).Run(std::vector<PermissionResult>(
        permissions.size(), PermissionResult(PermissionStatus::ASK)));
    return false;
  }

  // Verifies and evicts `render_frame_host` from BFcache. Returns true if
  // render_frame_host was evicted, returns false otherwise.
  if (render_frame_host->IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kRequestPermission)) {
    std::move(callback).Run(std::vector<PermissionResult>(
        permissions.size(), PermissionResult(PermissionStatus::ASK)));
    return false;
  }

  auto permission_results = std::vector<PermissionResult>();

  // Verify each permission independently to generate proper warning messages.
  bool is_permission_allowed = true;
  for (const auto& permission : permissions) {
    PermissionType permission_type =
        blink::PermissionDescriptorToPermissionType(permission);
    PermissionResult result =
        VerifyContextOfCurrentDocument(permission_type, render_frame_host);
    permission_results.push_back(result);

    if (result.status == PermissionStatus::DENIED) {
      switch (result.source) {
        case PermissionStatusSource::FENCED_FRAME:
          render_frame_host->GetOutermostMainFrame()->AddMessageToConsole(
              blink::mojom::ConsoleMessageLevel::kWarning,
              blink::GetPermissionString(permission_type) +
                  " permission has been blocked because it was requested "
                  "inside a fenced frame. Fenced frames don't currently "
                  "support permission requests.");
          break;
#if !BUILDFLAG(IS_ANDROID)
        case PermissionStatusSource::FEATURE_POLICY:
          render_frame_host->GetOutermostMainFrame()->AddMessageToConsole(
              blink::mojom::ConsoleMessageLevel::kWarning,
              blink::GetPermissionString(permission_type) +
                  " permission has been blocked because of a permissions "
                  "policy applied to the current document. See "
                  "https://crbug.com/414348233 for more details.");
          break;
#endif
        default:
          break;
      }

      is_permission_allowed = false;
    }
  }

  if (!is_permission_allowed) {
    std::move(callback).Run(permission_results);
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
    base::OnceCallback<void(const std::vector<PermissionResult>&)> original_cb,
    std::vector<std::optional<PermissionResult>> overridden_results,
    const std::vector<PermissionResult>& delegated_results) {
  std::vector<PermissionResult> full_results;
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
    base::OnceCallback<void(PermissionResult)> callback,
    const std::vector<PermissionResult>& vector) {
  DCHECK_EQ(1ul, vector.size());
  std::move(callback).Run(vector.at(0));
}

// Removes from |description.permissions| the entries that have an override
// status (as per the provided overrides). Returns a result vector that contains
// all the statuses for permissions after applying overrides, using `nullopt`
// for those permissions that do not have an override.
std::vector<std::optional<PermissionResult>> OverridePermissions(
    PermissionRequestDescription& description,
    RenderFrameHost* render_frame_host,
    const PermissionOverrides& permission_overrides) {
  std::vector<blink::mojom::PermissionDescriptorPtr>
      permissions_without_overrides;
  std::vector<std::optional<PermissionResult>> results;

  for (const auto& permission : description.permissions) {
    std::optional<PermissionResult> override_status = permission_overrides.Get(
        render_frame_host->GetLastCommittedOrigin(),
        render_frame_host->GetMainFrame()->GetLastCommittedOrigin(),
        blink::PermissionDescriptorToPermissionType(permission));
    if (!override_status) {
      permissions_without_overrides.push_back(permission.Clone());
    }
    results.push_back(override_status);
  }

  description.permissions = std::move(permissions_without_overrides);
  return results;
}

ContentSettingsType ConvertPermissionTypeForCookieManager(
    PermissionType permission) {
  switch (permission) {
    case PermissionType::STORAGE_ACCESS_GRANT:
      return ContentSettingsType::STORAGE_ACCESS;
    case PermissionType::TOP_LEVEL_STORAGE_ACCESS:
      return ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS;
    default:
      NOTREACHED();
  }
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

PermissionResult PermissionControllerImpl::GetSubscriptionCurrentResult(
    const PermissionResultSubscription& subscription) {
  // The RFH may be null if the request is for a worker.
  RenderFrameHost* rfh = RenderFrameHost::FromID(subscription.render_process_id,
                                                 subscription.render_frame_id);
  if (rfh) {
    return GetPermissionResultForCurrentDocument(
        subscription.permission_descriptor, rfh);
  }

  RenderProcessHost* rph =
      RenderProcessHost::FromID(subscription.render_process_id);
  if (rph) {
    return GetPermissionResultForWorker(
        subscription.permission_descriptor, rph,
        url::Origin::Create(subscription.requesting_origin));
  }

  return GetPermissionResultInternal(subscription.permission_descriptor,
                                     subscription.requesting_origin,
                                     subscription.embedding_origin);
}

PermissionControllerImpl::SubscriptionsStatusMap
PermissionControllerImpl::GetSubscriptionsStatuses(
    const std::optional<GURL>& requesting_origin,
    const std::optional<GURL>& embedding_origin) {
  SubscriptionsStatusMap statuses;
  for (SubscriptionsMap::iterator iter(&subscriptions_); !iter.IsAtEnd();
       iter.Advance()) {
    PermissionResultSubscription* subscription = iter.GetCurrentValue();
    if (requesting_origin.has_value() && embedding_origin.has_value() &&
        subscription->requesting_origin != *requesting_origin &&
        subscription->embedding_origin != *embedding_origin) {
      continue;
    }
    statuses[iter.GetCurrentKey()] =
        GetSubscriptionCurrentResult(*subscription);
  }
  return statuses;
}

void PermissionControllerImpl::NotifyChangedSubscriptions(
    const SubscriptionsStatusMap& old_statuses) {
  std::vector<base::OnceClosure> callbacks;
  for (const auto& it : old_statuses) {
    auto key = it.first;
    PermissionResultSubscription* subscription = subscriptions_.Lookup(key);
    if (!subscription) {
      continue;
    }
    PermissionResult old_result = it.second;
    PermissionResult new_result = GetSubscriptionCurrentResult(*subscription);
    if (old_result != new_result) {
      // This is a private method that is called internally if a permission
      // status was set by DevTools. Suppress permission status override
      // verification and always notify listeners.
      callbacks.push_back(base::BindOnce(subscription->callback, new_result,
                                         /*ignore_status_override=*/true));
    }
  }
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

void PermissionControllerImpl::SetPermissionOverride(
    base::optional_ref<const url::Origin> requesting_origin,
    base::optional_ref<const url::Origin> embedding_origin,
    PermissionType permission,
    const PermissionStatus& status,
    base::OnceCallback<void(OverrideStatus)> callback) {
  CHECK_EQ(requesting_origin.has_value(), embedding_origin.has_value());

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate && !delegate->IsPermissionOverridable(
                      permission, requesting_origin, embedding_origin)) {
    std::move(callback).Run(OverrideStatus::kOverrideNotSet);
    return;
  }

  const std::optional<GURL> requesting_origin_url =
      requesting_origin.has_value()
          ? std::make_optional(requesting_origin->GetURL())
          : std::nullopt;
  const std::optional<GURL> embedding_origin_url =
      embedding_origin.has_value()
          ? std::make_optional(embedding_origin->GetURL())
          : std::nullopt;
  const auto old_statuses =
      GetSubscriptionsStatuses(requesting_origin_url, embedding_origin_url);

  permission_overrides_.Set(requesting_origin, embedding_origin, permission,
                            status);
  NotifyChangedSubscriptions(old_statuses);
  UpdateCookieManagerContentSettings(
      permission,
      base::BindOnce(std::move(callback), OverrideStatus::kOverrideSet));
}

void PermissionControllerImpl::GrantPermissionOverrides(
    base::optional_ref<const url::Origin> requesting_origin,
    base::optional_ref<const url::Origin> embedding_origin,
    const std::vector<PermissionType>& permissions,
    base::OnceCallback<void(OverrideStatus)> callback) {
  CHECK_EQ(requesting_origin.has_value(), embedding_origin.has_value());

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate) {
    for (const auto permission : permissions) {
      if (!delegate->IsPermissionOverridable(permission, requesting_origin,
                                             embedding_origin)) {
        std::move(callback).Run(OverrideStatus::kOverrideNotSet);
        return;
      }
    }
  }

  const std::optional<GURL> requesting_origin_url =
      requesting_origin.has_value()
          ? std::make_optional(requesting_origin->GetURL())
          : std::nullopt;
  const std::optional<GURL> embedding_origin_url =
      embedding_origin.has_value()
          ? std::make_optional(embedding_origin->GetURL())
          : std::nullopt;
  const auto old_statuses =
      GetSubscriptionsStatuses(requesting_origin_url, embedding_origin_url);

  permission_overrides_.GrantPermissions(requesting_origin, embedding_origin,
                                         permissions);
  // If any statuses changed because they lose overrides or the new overrides
  // modify their previous state (overridden or not), subscribers must be
  // notified manually.
  NotifyChangedSubscriptions(old_statuses);

  UpdateCookieManagerContentSettings(
      /*permission=*/std::nullopt,
      base::BindOnce(std::move(callback), OverrideStatus::kOverrideSet));
}

void PermissionControllerImpl::ResetPermissionOverrides(
    base::OnceClosure callback) {
  const auto old_statuses = GetSubscriptionsStatuses();
  permission_overrides_ = PermissionOverrides();

  // If any statuses changed because they lost their overrides, the subscribers
  // must be notified manually.
  NotifyChangedSubscriptions(old_statuses);

  UpdateCookieManagerContentSettings(/*permission=*/std::nullopt,
                                     std::move(callback));
}

void PermissionControllerImpl::RequestPermissions(
    RenderFrameHost* render_frame_host,
    PermissionRequestDescription request_description,
    base::OnceCallback<void(const std::vector<PermissionResult>&)> callback) {
  if (!IsRequestAllowed(request_description.permissions, render_frame_host,
                        callback)) {
    return;
  }

  for (const blink::mojom::PermissionDescriptorPtr& permission :
       request_description.permissions) {
    NotifySchedulerAboutPermissionRequest(
        render_frame_host,
        blink::PermissionDescriptorToPermissionType(permission));
  }

  std::vector<std::optional<PermissionResult>> override_results =
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
    std::move(wrapper).Run(std::vector<PermissionResult>(
        request_description.permissions.size(),
        PermissionResult(PermissionStatus::DENIED)));
    return;
  }

  delegate->RequestPermissions(render_frame_host, request_description,
                               std::move(wrapper));
}

void PermissionControllerImpl::RequestPermissionFromCurrentDocument(
    RenderFrameHost* render_frame_host,
    PermissionRequestDescription request_description,
    base::OnceCallback<void(PermissionResult)> callback) {
  RequestPermissionsFromCurrentDocument(
      render_frame_host, std::move(request_description),
      base::BindOnce(&PermissionStatusCallbackWrapper, std::move(callback)));
}

void PermissionControllerImpl::RequestPermissionsFromCurrentDocument(
    RenderFrameHost* render_frame_host,
    PermissionRequestDescription request_description,
    base::OnceCallback<void(const std::vector<PermissionResult>&)> callback) {
  if (!IsRequestAllowed(request_description.permissions, render_frame_host,
                        callback)) {
    return;
  }

  for (const auto& permission : request_description.permissions) {
    NotifySchedulerAboutPermissionRequest(
        render_frame_host,
        blink::PermissionDescriptorToPermissionType(permission));
  }

  request_description.requesting_origin =
      render_frame_host->GetLastCommittedOrigin().GetURL();
  std::vector<std::optional<PermissionResult>> override_results =
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
    std::move(wrapper).Run(std::vector<PermissionResult>(
        request_description.permissions.size(),
        PermissionResult(PermissionStatus::DENIED)));
    return;
  }

  delegate->RequestPermissionsFromCurrentDocument(
      render_frame_host, request_description, std::move(wrapper));
}

void PermissionControllerImpl::ResetPermission(blink::PermissionType permission,
                                               const url::Origin& origin) {
  ResetPermission(permission, origin.GetURL(), origin.GetURL());
}

void PermissionControllerImpl::UpdateCookieManagerContentSettings(
    std::optional<PermissionType> permission_to_process,
    base::OnceClosure callback) {
  std::vector<PermissionType> permissions;
  if (permission_to_process == PermissionType::STORAGE_ACCESS_GRANT ||
      permission_to_process == PermissionType::TOP_LEVEL_STORAGE_ACCESS) {
    permissions.push_back(permission_to_process.value());
  } else if (!permission_to_process) {
    // A null `permission_to_process` value indicates that a bulk operation
    // like `ResetPermissionOverrides` or `GrantPermissionOverrides` was
    // performed. In such cases, we must update all permissions that require
    // it, which currently includes both Storage Access API permissions.
    permissions = {PermissionType::STORAGE_ACCESS_GRANT,
                   PermissionType::TOP_LEVEL_STORAGE_ACCESS};
  }

  const auto callback_after_ipc =
      base::BarrierClosure(permissions.size(), std::move(callback));

  for (auto permission : permissions) {
    std::vector<ContentSettingPatternSource> grants =
        permission_overrides_.CreateContentSettingsForType(permission);

    // The network service does not care about non-allowed settings. So we can
    // filter those ones out here.
    std::erase_if(grants, [](const ContentSettingPatternSource& setting) {
      return setting.GetContentSetting() != CONTENT_SETTING_ALLOW;
    });

    browser_context_->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->SetContentSettings(ConvertPermissionTypeForCookieManager(permission),
                             grants, callback_after_ipc);
  }
}

PermissionResult PermissionControllerImpl::GetPermissionResultInternal(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  std::optional<PermissionResult> permission_result = permission_overrides_.Get(
      url::Origin::Create(requesting_origin),
      url::Origin::Create(embedding_origin),
      blink::PermissionDescriptorToPermissionType(permission_descriptor));
  if (permission_result) {
    return permission_result.value();
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionResult(PermissionStatus::DENIED);
  }
  return delegate->GetPermissionResultForOriginWithoutContext(
      permission_descriptor, url::Origin::Create(requesting_origin),
      url::Origin::Create(embedding_origin));
}

PermissionResult
PermissionControllerImpl::GetPermissionResultForCurrentDocumentInternal(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  auto permission_type =
      blink::PermissionDescriptorToPermissionType(permission_descriptor);
  std::optional<PermissionResult> permission_result = permission_overrides_.Get(
      render_frame_host->GetLastCommittedOrigin(),
      render_frame_host->GetMainFrame()->GetLastCommittedOrigin(),
      permission_type);
  if (permission_result) {
    return permission_result.value();
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionResult(PermissionStatus::DENIED);
  }

  PermissionResult result =
      VerifyContextOfCurrentDocument(permission_type, render_frame_host);
  if (result.status == PermissionStatus::DENIED) {
    return result;
  }

  return delegate->GetPermissionResultForCurrentDocument(
      permission_descriptor, render_frame_host, should_include_device_status);
}

PermissionStatus PermissionControllerImpl::GetPermissionStatusForWorker(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    RenderProcessHost* render_process_host,
    const url::Origin& worker_origin) {
  return GetPermissionResultForWorker(permission_descriptor,
                                      render_process_host, worker_origin)
      .status;
}

PermissionResult PermissionControllerImpl::GetPermissionResultForWorker(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    RenderProcessHost* render_process_host,
    const url::Origin& worker_origin) {
  auto permission_type =
      blink::PermissionDescriptorToPermissionType(permission_descriptor);

  // TODO(crbug.com/428178708): This is likely incorrect for partitioned
  // contexts and requires impact evaluation before updating to use embedding
  // and requesting origins.
  std::optional<PermissionResult> permission_result =
      permission_overrides_.Get(worker_origin, worker_origin, permission_type);
  if (permission_result) {
    return permission_result.value();
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionResult(PermissionStatus::DENIED);
  }

  return delegate->GetPermissionResultForWorker(
      permission_descriptor, render_process_host, worker_origin.GetURL());
}

PermissionStatus
PermissionControllerImpl::GetPermissionStatusForCurrentDocument(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    RenderFrameHost* render_frame_host) {
  return GetPermissionResultForCurrentDocument(permission_descriptor,
                                               render_frame_host)
      .status;
}

PermissionResult
PermissionControllerImpl::GetPermissionResultForCurrentDocument(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    RenderFrameHost* render_frame_host) {
  return GetPermissionResultForCurrentDocumentInternal(
      permission_descriptor, render_frame_host,
      /*should_include_device_status=*/false);
}

PermissionResult
PermissionControllerImpl::GetPermissionResultForOriginWithoutContext(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const url::Origin& origin) {
  return GetPermissionResultForOriginWithoutContext(permission_descriptor,
                                                    origin, origin);
}

PermissionResult
PermissionControllerImpl::GetPermissionResultForOriginWithoutContext(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  auto permission_type =
      blink::PermissionDescriptorToPermissionType(permission_descriptor);
  std::optional<PermissionResult> permission_result = permission_overrides_.Get(
      requesting_origin, embedding_origin, permission_type);
  if (permission_result) {
    return permission_result.value();
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionResult(PermissionStatus::DENIED);
  }

  return delegate->GetPermissionResultForOriginWithoutContext(
      permission_descriptor, requesting_origin, embedding_origin);
}

PermissionResult
PermissionControllerImpl::GetPermissionResultForEmbeddedRequester(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    RenderFrameHost* render_frame_host,
    const url::Origin& requesting_origin) {
  auto permission_type =
      blink::PermissionDescriptorToPermissionType(permission_descriptor);

  // This API is suited only for `TOP_LEVEL_STORAGE_ACCESS`. Do not use it for
  // other permissions unless discussed with `permissions-core@`.
  DCHECK(permission_type == blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS);

  if (permission_type != blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS) {
    return PermissionResult(PermissionStatus::DENIED);
  }

  std::optional<PermissionResult> permission_result = permission_overrides_.Get(
      requesting_origin,
      render_frame_host->GetMainFrame()->GetLastCommittedOrigin(),
      permission_type);
  if (permission_result) {
    return permission_result.value();
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    return PermissionResult(PermissionStatus::DENIED);
  }

  if (VerifyContextOfCurrentDocument(permission_type, render_frame_host)
          .status == PermissionStatus::DENIED) {
    return PermissionResult(PermissionStatus::DENIED);
  }

  return delegate->GetPermissionResultForEmbeddedRequester(
      permission_descriptor, render_frame_host, requesting_origin);
}

PermissionStatus PermissionControllerImpl::GetCombinedPermissionAndDeviceStatus(
    const blink::mojom::PermissionDescriptorPtr& permission,
    RenderFrameHost* render_frame_host) {
  CHECK(PermissionUtil::IsDevicePermission(permission)) << permission->name;
  return GetPermissionResultForCurrentDocumentInternal(
             permission, render_frame_host,
             /*should_include_device_status=*/true)
      .status;
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

void PermissionControllerImpl::PermissionResultChange(
    const base::RepeatingCallback<void(PermissionResult)>& callback,
    SubscriptionId subscription_id,
    PermissionResult result,
    bool ignore_status_override) {
  // Check if the permission status override should be ignored. The verification
  // is suppressed if a permission status change was initiated by DevTools. In
  // all other cases permission status override is always checked.
  if (ignore_status_override) {
    callback.Run(result);
    return;
  }
  PermissionResultSubscription* subscription =
      subscriptions_.Lookup(subscription_id);
  DCHECK(subscription);
  // TODO(crbug.com/40056329) Adding this block to prevent crashes while we
  // investigate the root cause of the crash. This block will be removed as the
  // DCHECK() above should be enough.
  if (!subscription) {
    return;
  }
  std::optional<PermissionResult> permission_result = permission_overrides_.Get(
      url::Origin::Create(subscription->requesting_origin),
      url::Origin::Create(subscription->embedding_origin),
      blink::PermissionDescriptorToPermissionType(
          subscription->permission_descriptor));
  if (!permission_result.has_value()) {
    callback.Run(result);
  }
}

PermissionController::SubscriptionId
PermissionControllerImpl::SubscribeToPermissionResultChange(
    blink::mojom::PermissionDescriptorPtr permission_descriptor,
    RenderProcessHost* render_process_host,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool should_include_device_status,
    const base::RepeatingCallback<void(PermissionResult)>& callback) {
  DCHECK(!render_process_host || !render_frame_host);

  auto id = subscription_id_generator_.GenerateNextId();
  auto subscription = std::make_unique<PermissionResultSubscription>(
      std::move(permission_descriptor));
  subscription->callback =
      base::BindRepeating(&PermissionControllerImpl::PermissionResultChange,
                          base::Unretained(this), callback, id);
  subscription->requesting_origin = requesting_origin;
  subscription->should_include_device_status = should_include_device_status;

  // The RFH may be null if the request is for a worker.
  if (render_frame_host) {
    subscription->embedding_origin =
        PermissionUtil::GetLastCommittedOriginAsURL(
            render_frame_host->GetMainFrame());
    subscription->render_frame_id = render_frame_host->GetRoutingID();
    subscription->render_process_id =
        render_frame_host->GetProcess()->GetDeprecatedID();
  } else {
    subscription->embedding_origin = requesting_origin;
    subscription->render_frame_id = -1;
    subscription->render_process_id =
        render_process_host ? render_process_host->GetDeprecatedID() : -1;
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

void PermissionControllerImpl::UnsubscribeFromPermissionResultChange(
    SubscriptionId subscription_id) {
  PermissionResultSubscription* subscription =
      subscriptions_.Lookup(subscription_id);
  if (!subscription) {
    return;
  }
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate) {
    delegate->UnsubscribeFromPermissionResultChange(subscription_id);
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
