// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_header_observer.h"

#include <deque>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/bad_message.h"
#include "content/browser/navigation_or_document_handle.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_lock_manager.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/shared_storage_utils.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"

namespace content {

namespace {

using MethodWithOptionsPtr =
    network::mojom::SharedStorageModifierMethodWithOptionsPtr;
using ContextType = StoragePartitionImpl::ContextType;
using AccessScope = blink::SharedStorageAccessScope;

bool IsSharedStorageAllowedByPermissionsPolicy(
    SharedStorageHeaderObserver::PermissionsPolicyDoubleCheckStatus
        permissions_policy_status) {
  return permissions_policy_status ==
             SharedStorageHeaderObserver::PermissionsPolicyDoubleCheckStatus::
                 kNavigationSourceNoPolicy ||
         permissions_policy_status ==
             SharedStorageHeaderObserver::PermissionsPolicyDoubleCheckStatus::
                 kEnabled;
}

GlobalRenderFrameHostId GetMainFrameIdFromRFH(RenderFrameHost* rfh) {
  return static_cast<RenderFrameHostImpl*>(rfh->GetOutermostMainFrame())
      ->GetGlobalId();
}

GlobalRenderFrameHostId GetMainFrameIdFromNavigationOrDocumentHandle(
    NavigationOrDocumentHandle* navigation_or_document_handle) {
  if (auto* navigation_request =
          navigation_or_document_handle->GetNavigationRequest()) {
    return GetMainFrameIdFromRFH(
        navigation_request->frame_tree_node()->current_frame_host());
  }
  if (auto* rfh = navigation_or_document_handle->GetDocument()) {
    return GetMainFrameIdFromRFH(rfh);
  }
  return GlobalRenderFrameHostId();
}

}  // namespace

SharedStorageHeaderObserver::SharedStorageHeaderObserver(
    StoragePartitionImpl* storage_partition)
    : storage_partition_(storage_partition) {}

SharedStorageHeaderObserver::~SharedStorageHeaderObserver() = default;

void SharedStorageHeaderObserver::HeaderReceived(
    const url::Origin& request_origin,
    ContextType context_type,
    NavigationOrDocumentHandle* navigation_or_document_handle,
    std::vector<MethodWithOptionsPtr> methods_with_options,
    const std::optional<std::string>& with_lock,
    base::OnceClosure callback,
    mojo::ReportBadMessageCallback bad_message_callback,
    bool can_defer) {
  DCHECK(callback);
  DCHECK(bad_message_callback);

  if (!network::IsOriginPotentiallyTrustworthy(request_origin)) {
    // This could indicate a compromised renderer or network service, since we
    // already required a secure context in any previous checks. Terminate the
    // network service.
    std::move(bad_message_callback)
        .Run(
            "Shared-Storage-Write is only available for origins deemed "
            "potentially trustworthy.");
    if (context_type == ContextType::kRenderFrameHostContext) {
      // The request was from a subresource source, so we should also terminate
      // the renderer.
      bad_message::ReceivedBadMessage(
          navigation_or_document_handle->GetDocument()->GetProcess(),
          bad_message::BadMessageReason::
              SSHO_RECEIVED_SHARED_STORAGE_WRITE_HEADER_FROM_UNTRUSTWORTHY_ORIGIN);
    }
    return;
  }

  if (request_origin.opaque()) {
    // This could indicate a compromised renderer or network service, since we
    // already required a non-opaque origin in any previous checks. Terminate
    // the network service.
    std::move(bad_message_callback)
        .Run("Shared-Storage-Write is not available for opaque origins.");
    if (context_type == ContextType::kRenderFrameHostContext) {
      // The request was from a subresource source, so we should also terminate
      // the renderer.
      bad_message::ReceivedBadMessage(
          navigation_or_document_handle->GetDocument()->GetProcess(),
          bad_message::BadMessageReason::
              SSHO_RECEIVED_SHARED_STORAGE_WRITE_HEADER_FROM_OPAQUE_ORIGIN);
    }
    return;
  }

  PermissionsPolicyDoubleCheckStatus permissions_policy_status =
      DoPermissionsPolicyDoubleCheck(request_origin, context_type,
                                     navigation_or_document_handle);
  base::UmaHistogramEnumeration(
      "Storage.SharedStorage.HeaderObserver.PermissionsPolicyDoubleCheckStatus",
      permissions_policy_status);

  switch (permissions_policy_status) {
    case PermissionsPolicyDoubleCheckStatus::kDisabled: {
      // Since we have previously checked `PermissionsPolicy` and the feature
      // was said to be enabled then, the discrepancy may indicate a compromised
      // renderer or network service. Terminate the network service.
      std::move(bad_message_callback)
          .Run(
              "Permissions-Policy denied permission to Shared-Storage-Write "
              "operations.");
      if (context_type == ContextType::kRenderFrameHostContext) {
        // The request was from a subresource source, so we should also
        // terminate the renderer.
        bad_message::ReceivedBadMessage(
            navigation_or_document_handle->GetDocument()->GetProcess(),
            bad_message::BadMessageReason::
                SSHO_RECEIVED_SHARED_STORAGE_WRITE_HEADER_WITH_PERMISSION_DISABLED);
      }
      return;
    }
    case PermissionsPolicyDoubleCheckStatus::kDisallowedMainFrameNavigation: {
      // This could indicate a compromised network service, as we previously
      // marked any main frame navigations as ineligible for shared storage in
      // `NavigationRequest` before sending information to the network service.
      // Terminate the network service.
      std::move(bad_message_callback)
          .Run(
              "Shared-Storage-Write is not available for main frame "
              "navigations.");
      return;
    }
    case PermissionsPolicyDoubleCheckStatus::kSubresourceSourceDefer: {
      auto* rfh = navigation_or_document_handle->GetDocument();
      if (rfh && can_defer) {
        // We are in a case where the double check is required, and yet we're
        // unable to complete it yet because the RFH has not yet committed. We
        // may be able to complete it once the RFH commits, so defer until this
        // possibly happens. Note that in the `defer_callback`, we set
        // `can_defer` to false in order to prevent any subsequent deferral for
        // this header.
        base::OnceCallback<void(NavigationOrDocumentHandle*)> defer_callback =
            base::BindOnce(
                [](base::WeakPtr<SharedStorageHeaderObserver> header_observer,
                   const url::Origin& request_origin, ContextType context_type,
                   std::vector<MethodWithOptionsPtr> methods_with_options,
                   const std::optional<std::string>& with_lock,
                   mojo::ReportBadMessageCallback bad_message_callback,
                   NavigationOrDocumentHandle* navigation_or_document_handle) {
                  if (header_observer) {
                    header_observer->HeaderReceived(
                        request_origin, context_type,
                        navigation_or_document_handle,
                        std::move(methods_with_options), with_lock,
                        base::DoNothing(), std::move(bad_message_callback),
                        /*can_defer=*/false);
                  }
                },
                weak_ptr_factory_.GetWeakPtr(), request_origin, context_type,
                std::move(methods_with_options), with_lock,
                std::move(bad_message_callback));
        static_cast<RenderFrameHostImpl*>(rfh)
            ->AddDeferredSharedStorageHeaderCallback(std::move(defer_callback));
      }
      std::move(callback).Run();
      return;
    }
    case PermissionsPolicyDoubleCheckStatus::
        kSubresourceSourceOtherLifecycleState:
      [[fallthrough]];
    case PermissionsPolicyDoubleCheckStatus::kSubresourceSourceNoRFH:
      [[fallthrough]];
    case PermissionsPolicyDoubleCheckStatus::kSubresourceSourceNoPolicy:
      // We are in a case where the double check is required, and yet we're
      // unable to complete it due to either a lack of RFH, a lack of policy, or
      // a RFH in a LifecycleState other than kPendingCommit or kActive. We have
      // no reason to suspect a compromised renderer or network service, so we
      // simply drop any methods and return to the network service.
      std::move(callback).Run();
      return;
    case PermissionsPolicyDoubleCheckStatus::kNavigationSourceNoPolicy:
      // We can allow this case to proceed because `NavigationRequest` completed
      // the previous permissions policy checks in the browser process.
      break;
    case PermissionsPolicyDoubleCheckStatus::kEnabled:
      // This is the expected outcome in most cases.
      break;
    default:
      NOTREACHED();
  }

  CHECK(IsSharedStorageAllowedByPermissionsPolicy(permissions_policy_status));

  if (!IsSharedStorageAllowedBySiteSettings(navigation_or_document_handle,
                                            request_origin,
                                            /*out_debug_message=*/nullptr)) {
    // TODO(crbug.com/40064101):
    // 1. Log the following error message to console:
    // "'Shared-Storage-Write: shared storage is disabled."
    // 2. Send a non-null `out_debug_message` param and append it to the above
    // error message if the value of
    // `network::features::kSharedStorageExposeDebugMessageForSettingsStatus`
    // is true.
    std::move(callback).Run();
    return;
  }

  GlobalRenderFrameHostId main_frame_id =
      GetMainFrameIdFromNavigationOrDocumentHandle(
          navigation_or_document_handle);

  std::vector<MethodWithOptionsPtr> cloned_methods_with_options;
  cloned_methods_with_options.reserve(methods_with_options.size());

  for (auto& method_with_options : methods_with_options) {
    cloned_methods_with_options.push_back(method_with_options.Clone());
  }

  storage_partition_->GetSharedStorageRuntimeManager()
      ->lock_manager()
      .SharedStorageBatchUpdate(
          std::move(methods_with_options), with_lock, request_origin,
          AccessScope::kHeader, main_frame_id,
          /*worklet_devtools_token=*/base::UnguessableToken::Null(),
          base::BindOnce(&SharedStorageHeaderObserver::OnBatchUpdateFinished,
                         weak_ptr_factory_.GetWeakPtr(), request_origin,
                         std::move(cloned_methods_with_options), with_lock));

  OnHeaderProcessed(request_origin);
  std::move(callback).Run();
}

SharedStorageHeaderObserver::PermissionsPolicyDoubleCheckStatus
SharedStorageHeaderObserver::DoPermissionsPolicyDoubleCheck(
    const url::Origin& request_origin,
    ContextType context_type,
    NavigationOrDocumentHandle* navigation_or_document_handle) {
  switch (context_type) {
    case ContextType::kRenderFrameHostContext: {
      auto* rfh = navigation_or_document_handle->GetDocument();
      if (!rfh) {
        // This request may arrive after the document is destroyed. We consider
        // this case to be ineligible for writing to shared storage.
        // TODO(cammie): Investigate why a test unexpectedly hit this condition,
        // e.g. "All/SharedStorageHeaderPrefBrowserTest.Basic/"
        // + "PrivacySandboxDisabled_3PCookiesAllowed_*" on android-arm64-rel.
        return PermissionsPolicyDoubleCheckStatus::kSubresourceSourceNoRFH;
      }
      if (rfh->IsInLifecycleState(
              RenderFrameHost::LifecycleState::kPendingCommit)) {
        // Due to the race between the subresource requests and navigations,
        // this request may arrive before the commit confirmation is received.
        // We can defer and try again later if a corresponding commit
        // notification is received.
        return PermissionsPolicyDoubleCheckStatus::kSubresourceSourceDefer;
      }
      if (!rfh->IsInLifecycleState(RenderFrameHost::LifecycleState::kActive)) {
        // The RenderFrameHost is in a lifecycle state where it is unclear
        // if/how we could properly handle the required permissions policy
        // check. So we drop these methods.
        // TODO(cammie): Investigate further whether any of these cases can be
        // handled.
        return PermissionsPolicyDoubleCheckStatus::
            kSubresourceSourceOtherLifecycleState;
      }
      auto* permissions_policy =
          static_cast<RenderFrameHostImpl*>(rfh)->GetPermissionsPolicy();
      if (!permissions_policy) {
        // If we cannot obtain a permissions policy, we consider the request to
        // be ineligible for writing to shared storage.
        return PermissionsPolicyDoubleCheckStatus::kSubresourceSourceNoPolicy;
      }

      // `network::mojom::PermissionsPolicyFeature::kSharedStorage` should be
      // treated as an assumed opt-in feature during the permissions check.
      return permissions_policy->IsFeatureEnabledForOrigin(
                 network::mojom::PermissionsPolicyFeature::kSharedStorage,
                 request_origin, /*override_default_policy_to_all=*/true)
                 ? PermissionsPolicyDoubleCheckStatus::kEnabled
                 : PermissionsPolicyDoubleCheckStatus::kDisabled;
    }
    case ContextType::kNavigationRequestContext: {
      auto* frame_tree_node = navigation_or_document_handle->GetFrameTreeNode();

      if (!frame_tree_node->parent()) {
        return PermissionsPolicyDoubleCheckStatus::
            kDisallowedMainFrameNavigation;
      }
      const network::PermissionsPolicy* parent_policy =
          frame_tree_node->parent()->GetPermissionsPolicy();
      if (!parent_policy) {
        return PermissionsPolicyDoubleCheckStatus::kNavigationSourceNoPolicy;
      }
      return parent_policy->IsFeatureEnabledForOrigin(
                 network::mojom::PermissionsPolicyFeature::kSharedStorage,
                 request_origin)
                 ? PermissionsPolicyDoubleCheckStatus::kEnabled
                 : PermissionsPolicyDoubleCheckStatus::kDisabled;
    }
    case ContextType::kSharedOrServiceWorkerContext:
      // TODO(cammie): Handle the service worker case. Currently, the headers
      // aren't available to requests initiated by service workers.
      [[fallthrough]];
    default:
      NOTREACHED();
  }
}

bool SharedStorageHeaderObserver::IsSharedStorageAllowedBySiteSettings(
    NavigationOrDocumentHandle* navigation_or_document_handle,
    const url::Origin& request_origin,
    std::string* out_debug_message) {
  DCHECK(storage_partition_);
  DCHECK(storage_partition_->browser_context());

  bool has_top_frame_origin =
      navigation_or_document_handle &&
      navigation_or_document_handle->GetTopmostFrameOrigin().has_value();
  url::Origin top_frame_origin =
      has_top_frame_origin
          ? navigation_or_document_handle->GetTopmostFrameOrigin().value()
          : url::Origin();
  base::UmaHistogramBoolean(
      "Storage.SharedStorage.HeaderObserver.CreatedOpaqueOriginForPrefsCheck",
      !has_top_frame_origin);

  auto* rfh = navigation_or_document_handle
                  ? navigation_or_document_handle->GetDocument()
                  : nullptr;
  return GetContentClient()->browser()->IsSharedStorageAllowed(
      storage_partition_->browser_context(), rfh, top_frame_origin,
      request_origin, out_debug_message,
      /*out_block_is_site_setting_specific=*/nullptr);
}

}  // namespace content
