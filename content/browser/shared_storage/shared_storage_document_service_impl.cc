// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_document_service_impl.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/strings/strcat.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Note that this function would also return false if the context origin is
// opaque. This is stricter than the web platform's notion of "secure context".
// TODO(yaoxia): This should be function in FrameTreeNode.
bool IsSecureFrame(RenderFrameHost* frame) {
  while (frame) {
    if (!network::IsOriginPotentiallyTrustworthy(
            frame->GetLastCommittedOrigin())) {
      return false;
    }
    frame = frame->GetParent();
  }
  return true;
}

using AccessType =
    SharedStorageWorkletHostManager::SharedStorageObserverInterface::AccessType;

using OperationResult = storage::SharedStorageManager::OperationResult;
using GetResult = storage::SharedStorageManager::GetResult;

}  // namespace

const char kSharedStorageDisabledMessage[] = "sharedStorage is disabled";

const char kSharedStorageSelectURLDisabledMessage[] =
    "sharedStorage.selectURL is disabled";

const char kSharedStorageAddModuleDisabledMessage[] =
    "sharedStorage.worklet.addModule is disabled because either sharedStorage "
    "is disabled or both sharedStorage.selectURL and privateAggregation are "
    "disabled";

const char kSharedStorageSelectURLLimitReachedMessage[] =
    "sharedStorage.selectURL limit has been reached";

// NOTE: To preserve user privacy, the default value of the
// `blink::features::kSharedStorageExposeDebugMessageForSettingsStatus`
// feature param MUST remain set to false (although the value can be overridden
// via the command line or in tests).
std::string GetSharedStorageErrorMessage(const std::string& debug_message,
                                         const std::string& input_message) {
  return blink::features::kSharedStorageExposeDebugMessageForSettingsStatus
                 .Get()
             ? base::StrCat({input_message, "\nDebug: ", debug_message})
             : input_message;
}

SharedStorageDocumentServiceImpl::~SharedStorageDocumentServiceImpl() {
  GetSharedStorageWorkletHostManager()->OnDocumentServiceDestroyed(this);
}

void SharedStorageDocumentServiceImpl::Bind(
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageDocumentService>
        receiver) {
  CHECK(!receiver_)
      << "Multiple attempts to bind the SharedStorageDocumentService receiver";

  if (render_frame_host().GetLastCommittedOrigin().opaque()) {
    mojo::ReportBadMessage(
        "Attempted to request SharedStorageDocumentService from an opaque "
        "origin context");
    return;
  }

  bool is_secure_frame = IsSecureFrame(&render_frame_host());

  base::UmaHistogramBoolean(
      "Storage.SharedStorage.DocumentServiceBind.IsSecureFrame",
      is_secure_frame);

  if (!is_secure_frame) {
    // TODO(crbug.com/40068897): Invoke mojo::ReportBadMessage here when
    // we can be sure honest renderers won't hit this path.
    return;
  }

  receiver_.Bind(std::move(receiver));
}

void SharedStorageDocumentServiceImpl::CreateWorklet(
    const GURL& script_source_url,
    network::mojom::CredentialsMode credentials_mode,
    const std::vector<blink::mojom::OriginTrialFeature>& origin_trial_features,
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host,
    CreateWorkletCallback callback) {
  // A document can only create multiple worklets with `kSharedStorageAPIM125`
  // enabled.
  if (!base::FeatureList::IsEnabled(blink::features::kSharedStorageAPIM125)) {
    if (create_worklet_called_) {
      // This could indicate a compromised renderer, so let's terminate it.
      receiver_.ReportBadMessage("Attempted to create multiple worklets.");
      LogSharedStorageWorkletError(
          blink::SharedStorageWorkletErrorType::
              kAddModuleNonWebVisibleMulipleWorkletsDisabled);
      return;
    }
  }

  create_worklet_called_ = true;
  bool is_same_origin =
      render_frame_host().GetLastCommittedOrigin().IsSameOriginWith(
          script_source_url);

  // A document can only create cross-origin worklets with
  // `kSharedStorageAPIM125` enabled.
  if (!base::FeatureList::IsEnabled(blink::features::kSharedStorageAPIM125) &&
      !is_same_origin) {
    // This could indicate a compromised renderer, so let's terminate it.
    receiver_.ReportBadMessage(
        "Attempted to load a cross-origin module script.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::
            kAddModuleNonWebVisibleCrossOriginWorkletsDisabled);
    return;
  }

  std::string debug_message;
  bool prefs_failure_is_site_specific = false;
  bool prefs_success = IsSharedStorageAddModuleAllowedForOrigin(
      url::Origin::Create(script_source_url), &debug_message,
      &prefs_failure_is_site_specific);

  if (!prefs_success && (is_same_origin || !prefs_failure_is_site_specific)) {
    OnCreateWorkletResponseIntercepted(
        is_same_origin,
        /*prefs_success=*/false, prefs_failure_is_site_specific,
        std::move(callback),
        /*post_prefs_success=*/false,
        /*error_message=*/
        GetSharedStorageErrorMessage(debug_message,
                                     kSharedStorageAddModuleDisabledMessage));
    return;
  }

  GetSharedStorageWorkletHostManager()->CreateWorkletHost(
      this, render_frame_host().GetLastCommittedOrigin(), script_source_url,
      credentials_mode, origin_trial_features, std::move(worklet_host),
      base::BindOnce(
          &SharedStorageDocumentServiceImpl::OnCreateWorkletResponseIntercepted,
          weak_ptr_factory_.GetWeakPtr(), is_same_origin, prefs_success,
          prefs_failure_is_site_specific, std::move(callback)));
}

void SharedStorageDocumentServiceImpl::SharedStorageGet(
    const std::u16string& key,
    SharedStorageGetCallback callback) {
  if (!render_frame_host().IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "Attempted to call get() outside of a fenced frame.");
    return;
  }

  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(blink::mojom::SharedStorageGetStatus::kError,
                            /*error_message=*/
                            GetSharedStorageErrorMessage(
                                debug_message, kSharedStorageDisabledMessage),
                            /*value=*/{});
    return;
  }

  if (!(static_cast<RenderFrameHostImpl&>(render_frame_host())
            .CanReadFromSharedStorage())) {
    std::move(callback).Run(
        blink::mojom::SharedStorageGetStatus::kError,
        /*error_message=*/
        "sharedStorage.get() is not allowed in a fenced frame until network "
        "access for it and all descendent frames has been revoked with "
        "window.fence.disableUntrustedNetwork()",
        /*value=*/{});
    return;
  }

  GetSharedStorageWorkletHostManager()->NotifySharedStorageAccessed(
      AccessType::kDocumentGet, main_frame_id(), SerializeLastCommittedOrigin(),
      SharedStorageEventParams::CreateForGetOrDelete(base::UTF16ToUTF8(key)));

  auto operation_completed_callback = base::BindOnce(
      [](SharedStorageGetCallback callback, GetResult result) {
        // If the key is not found but there is no other error, the worklet will
        // resolve the promise to undefined.
        if (result.result == OperationResult::kNotFound ||
            result.result == OperationResult::kExpired) {
          std::move(callback).Run(
              blink::mojom::SharedStorageGetStatus::kNotFound,
              /*error_message=*/"sharedStorage.get() could not find key",
              /*value=*/{});
          return;
        }

        if (result.result != OperationResult::kSuccess) {
          std::move(callback).Run(
              blink::mojom::SharedStorageGetStatus::kError,
              /*error_message=*/"sharedStorage.get() failed", /*value=*/{});
          return;
        }

        std::move(callback).Run(blink::mojom::SharedStorageGetStatus::kSuccess,
                                /*error_message=*/{}, /*value=*/result.data);
      },
      std::move(callback));

  GetSharedStorageManager()->Get(render_frame_host().GetLastCommittedOrigin(),
                                 key, std::move(operation_completed_callback));
}

void SharedStorageDocumentServiceImpl::SharedStorageSet(
    const std::u16string& key,
    const std::u16string& value,
    bool ignore_if_present,
    SharedStorageSetCallback callback) {
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/GetSharedStorageErrorMessage(
            debug_message, kSharedStorageDisabledMessage));
    return;
  }

  storage::SharedStorageDatabase::SetBehavior set_behavior =
      ignore_if_present
          ? storage::SharedStorageDatabase::SetBehavior::kIgnoreIfPresent
          : storage::SharedStorageDatabase::SetBehavior::kDefault;

  GetSharedStorageWorkletHostManager()->NotifySharedStorageAccessed(
      AccessType::kDocumentSet, main_frame_id(), SerializeLastCommittedOrigin(),
      SharedStorageEventParams::CreateForSet(
          base::UTF16ToUTF8(key), base::UTF16ToUTF8(value), ignore_if_present));

  GetSharedStorageManager()->Set(render_frame_host().GetLastCommittedOrigin(),
                                 key, value, base::DoNothing(), set_behavior);
  std::move(callback).Run(/*success=*/true, /*error_message=*/{});
}

void SharedStorageDocumentServiceImpl::SharedStorageAppend(
    const std::u16string& key,
    const std::u16string& value,
    SharedStorageAppendCallback callback) {
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/GetSharedStorageErrorMessage(
            debug_message, kSharedStorageDisabledMessage));
    return;
  }

  GetSharedStorageWorkletHostManager()->NotifySharedStorageAccessed(
      AccessType::kDocumentAppend, main_frame_id(),
      SerializeLastCommittedOrigin(),
      SharedStorageEventParams::CreateForAppend(base::UTF16ToUTF8(key),
                                                base::UTF16ToUTF8(value)));

  GetSharedStorageManager()->Append(
      render_frame_host().GetLastCommittedOrigin(), key, value,
      base::DoNothing());
  std::move(callback).Run(/*success=*/true, /*error_message=*/{});
}

void SharedStorageDocumentServiceImpl::SharedStorageDelete(
    const std::u16string& key,
    SharedStorageDeleteCallback callback) {
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/GetSharedStorageErrorMessage(
            debug_message, kSharedStorageDisabledMessage));
    return;
  }

  GetSharedStorageWorkletHostManager()->NotifySharedStorageAccessed(
      AccessType::kDocumentDelete, main_frame_id(),
      SerializeLastCommittedOrigin(),
      SharedStorageEventParams::CreateForGetOrDelete(base::UTF16ToUTF8(key)));

  GetSharedStorageManager()->Delete(
      render_frame_host().GetLastCommittedOrigin(), key, base::DoNothing());
  std::move(callback).Run(/*success=*/true, /*error_message=*/{});
}

void SharedStorageDocumentServiceImpl::SharedStorageClear(
    SharedStorageClearCallback callback) {
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/GetSharedStorageErrorMessage(
            debug_message, kSharedStorageDisabledMessage));
    return;
  }

  GetSharedStorageWorkletHostManager()->NotifySharedStorageAccessed(
      AccessType::kDocumentClear, main_frame_id(),
      SerializeLastCommittedOrigin(),
      SharedStorageEventParams::CreateDefault());

  GetSharedStorageManager()->Clear(render_frame_host().GetLastCommittedOrigin(),
                                   base::DoNothing());
  std::move(callback).Run(/*success=*/true, /*error_message=*/{});
}

base::WeakPtr<SharedStorageDocumentServiceImpl>
SharedStorageDocumentServiceImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

SharedStorageDocumentServiceImpl::SharedStorageDocumentServiceImpl(
    RenderFrameHost* rfh)
    : DocumentUserData<SharedStorageDocumentServiceImpl>(rfh),
      main_frame_origin_(
          rfh->GetOutermostMainFrame()->GetLastCommittedOrigin()),
      main_frame_id_(
          static_cast<RenderFrameHostImpl*>(rfh->GetOutermostMainFrame())
              ->GetFrameTreeNodeId()) {}

void SharedStorageDocumentServiceImpl::OnCreateWorkletResponseIntercepted(
    bool is_same_origin,
    bool prefs_success,
    bool prefs_failure_is_site_specific,
    CreateWorkletCallback original_callback,
    bool post_prefs_success,
    const std::string& error_message) {
  bool web_visible_prefs_error =
      !prefs_success && (is_same_origin || !prefs_failure_is_site_specific);
  bool other_web_visible_error = !post_prefs_success;

  if (web_visible_prefs_error || other_web_visible_error) {
    std::move(original_callback).Run(/*success=*/false, error_message);
    return;
  }

  // When the worklet and the worklet creator are not same-origin, the user
  // preferences for the worklet origin should not be revealed. So any
  // site-specific preference error will be suppressed.
  if (!prefs_success) {
    CHECK(!is_same_origin && prefs_failure_is_site_specific);
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::
            kAddModuleNonWebVisibleCrossOriginSharedStorageDisabled);
    std::move(original_callback).Run(/*success=*/true, /*error_message=*/{});
    return;
  }

  CHECK(post_prefs_success);
  LogSharedStorageWorkletError(blink::SharedStorageWorkletErrorType::kSuccess);
  std::move(original_callback).Run(/*success=*/true, /*error_message=*/{});
}

storage::SharedStorageManager*
SharedStorageDocumentServiceImpl::GetSharedStorageManager() {
  storage::SharedStorageManager* shared_storage_manager =
      static_cast<StoragePartitionImpl*>(
          render_frame_host().GetProcess()->GetStoragePartition())
          ->GetSharedStorageManager();

  // This `SharedStorageDocumentServiceImpl` is created only if
  // `kSharedStorageAPI` is enabled, in which case the `shared_storage_manager`
  // must be valid.
  DCHECK(shared_storage_manager);

  return shared_storage_manager;
}

SharedStorageWorkletHostManager*
SharedStorageDocumentServiceImpl::GetSharedStorageWorkletHostManager() {
  return static_cast<StoragePartitionImpl*>(
             render_frame_host().GetProcess()->GetStoragePartition())
      ->GetSharedStorageWorkletHostManager();
}

bool SharedStorageDocumentServiceImpl::IsSharedStorageAllowed(
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  // Will trigger a call to
  // `content_settings::PageSpecificContentSettings::BrowsingDataAccessed()` for
  // reporting purposes.
  return IsSharedStorageAllowedForOrigin(
      render_frame_host().GetLastCommittedOrigin(), out_debug_message,
      out_block_is_site_setting_specific);
}

bool SharedStorageDocumentServiceImpl::IsSharedStorageAllowedForOrigin(
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  // Will trigger a call to
  // `content_settings::PageSpecificContentSettings::BrowsingDataAccessed()` for
  // reporting purposes.
  return GetContentClient()->browser()->IsSharedStorageAllowed(
      render_frame_host().GetBrowserContext(), &render_frame_host(),
      main_frame_origin_, accessing_origin, out_debug_message,
      out_block_is_site_setting_specific);
}

bool SharedStorageDocumentServiceImpl::IsSharedStorageAddModuleAllowedForOrigin(
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  // Will trigger a call to
  // `content_settings::PageSpecificContentSettings::BrowsingDataAccessed()` for
  // reporting purposes.
  if (!IsSharedStorageAllowedForOrigin(accessing_origin, out_debug_message,
                                       out_block_is_site_setting_specific)) {
    return false;
  }

  bool select_url_block_is_site_setting_specific = false;
  bool private_aggregation_block_is_site_setting_specific = false;

  bool add_module_allowed =
      GetContentClient()->browser()->IsSharedStorageSelectURLAllowed(
          render_frame_host().GetBrowserContext(), main_frame_origin_,
          accessing_origin, out_debug_message,
          &select_url_block_is_site_setting_specific) ||
      GetContentClient()->browser()->IsPrivateAggregationAllowed(
          render_frame_host().GetBrowserContext(), main_frame_origin_,
          accessing_origin,
          &private_aggregation_block_is_site_setting_specific);

  *out_block_is_site_setting_specific =
      !add_module_allowed &&
      (select_url_block_is_site_setting_specific ||
       private_aggregation_block_is_site_setting_specific);
  return add_module_allowed;
}

std::string SharedStorageDocumentServiceImpl::SerializeLastCommittedOrigin()
    const {
  return render_frame_host().GetLastCommittedOrigin().Serialize();
}

DOCUMENT_USER_DATA_KEY_IMPL(SharedStorageDocumentServiceImpl);

}  // namespace content
