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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "url/url_constants.h"

namespace content {

namespace {

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

// TODO(crbug.com/1335504): Consider moving this function to
// third_party/blink/common/fenced_frame/fenced_frame_utils.cc.
bool IsValidFencedFrameReportingURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  return url.SchemeIs(url::kHttpsScheme);
}

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

const char kSharedStorageWorkletExpiredMessage[] =
    "The sharedStorage worklet cannot execute further operations because the "
    "previous operation did not include the option \'keepAlive: true\'.";

// static
bool& SharedStorageDocumentServiceImpl::
    GetBypassIsSharedStorageAllowedForTesting() {
  return GetBypassIsSharedStorageAllowed();
}

SharedStorageDocumentServiceImpl::~SharedStorageDocumentServiceImpl() {
  GetSharedStorageWorkletHostManager()->OnDocumentServiceDestroyed(this);
}

void SharedStorageDocumentServiceImpl::Bind(
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageDocumentService>
        receiver) {
  CHECK(!receiver_)
      << "Multiple attempts to bind the SharedStorageDocumentService receiver";

  if (!IsSecureFrame(&render_frame_host())) {
    // This could indicate a compromised renderer, so let's terminate it.
    mojo::ReportBadMessage(
        "Attempted to request SharedStorageDocumentService from an insecure "
        "context");
    return;
  }

  receiver_.Bind(std::move(receiver));
}

void SharedStorageDocumentServiceImpl::AddModuleOnWorklet(
    const GURL& script_source_url,
    AddModuleOnWorkletCallback callback) {
  if (!render_frame_host().GetLastCommittedOrigin().IsSameOriginWith(
          script_source_url)) {
    // This could indicate a compromised renderer, so let's terminate it.
    receiver_.ReportBadMessage(
        "Attempted to load a cross-origin module script.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kAddModuleNonWebVisible);
    return;
  }

  if (!IsSharedStorageAddModuleAllowed()) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageAddModuleDisabledMessage);
    return;
  }

  if (!keep_alive_worklet_after_operation_) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageWorkletExpiredMessage);
    return;
  }

  GetSharedStorageWorkletHostManager()->NotifySharedStorageAccessed(
      AccessType::kDocumentAddModule, main_frame_id(),
      SerializeLastCommittedOrigin(),
      SharedStorageEventParams::CreateForAddModule(script_source_url));

  // Initialize the `URLLoaderFactory` now, as later on the worklet may enter
  // keep-alive phase and won't have access to the `RenderFrameHost`.
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      frame_url_loader_factory;
  render_frame_host().CreateNetworkServiceDefaultFactory(
      frame_url_loader_factory.InitWithNewPipeAndPassReceiver());

  GetSharedStorageWorkletHost()->AddModuleOnWorklet(
      std::move(frame_url_loader_factory),
      render_frame_host().GetLastCommittedOrigin(), script_source_url,
      std::move(callback));
}

void SharedStorageDocumentServiceImpl::RunOperationOnWorklet(
    const std::string& name,
    blink::CloneableMessage serialized_data,
    bool keep_alive_after_operation,
    const absl::optional<std::string>& context_id,
    RunOperationOnWorkletCallback callback) {
  if (context_id.has_value() &&
      !blink::IsValidPrivateAggregationContextId(context_id.value())) {
    receiver_.ReportBadMessage("Invalid context_id.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kRunNonWebVisible);
    return;
  }

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(/*success=*/false,
                            /*error_message=*/kSharedStorageDisabledMessage);
    return;
  }

  if (!keep_alive_worklet_after_operation_) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageWorkletExpiredMessage);
    return;
  }

  keep_alive_worklet_after_operation_ = keep_alive_after_operation;

  GetSharedStorageWorkletHostManager()->NotifySharedStorageAccessed(
      AccessType::kDocumentRun, main_frame_id(), SerializeLastCommittedOrigin(),
      SharedStorageEventParams::CreateForRun(name, serialized_data));

  GetSharedStorageWorkletHost()->RunOperationOnWorklet(
      name, std::move(serialized_data), keep_alive_after_operation, context_id);
  std::move(callback).Run(/*success=*/true, /*error_message=*/{});
}

void SharedStorageDocumentServiceImpl::RunURLSelectionOperationOnWorklet(
    const std::string& name,
    std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
        urls_with_metadata,
    blink::CloneableMessage serialized_data,
    bool keep_alive_after_operation,
    const absl::optional<std::string>& context_id,
    RunURLSelectionOperationOnWorkletCallback callback) {
  if (!blink::IsValidSharedStorageURLsArrayLength(urls_with_metadata.size())) {
    // This could indicate a compromised renderer, so let's terminate it.
    receiver_.ReportBadMessage(
        "Attempted to execute RunURLSelectionOperationOnWorklet with invalid "
        "URLs array length.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible);
    return;
  }

  std::vector<SharedStorageEventParams::SharedStorageUrlSpecWithMetadata>
      converted_urls;
  for (const auto& url_with_metadata : urls_with_metadata) {
    // TODO(crbug.com/1318970): Use `blink::IsValidFencedFrameURL()` here.
    if (!url_with_metadata->url.is_valid()) {
      // This could indicate a compromised renderer, since the URLs were already
      // validated in the renderer.
      receiver_.ReportBadMessage(
          base::StrCat({"Invalid fenced frame URL '",
                        url_with_metadata->url.possibly_invalid_spec(), "'"}));
      LogSharedStorageWorkletError(
          blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible);
      return;
    }

    std::map<std::string, std::string> reporting_metadata;
    for (const auto& metadata_pair : url_with_metadata->reporting_metadata) {
      if (!IsValidFencedFrameReportingURL(metadata_pair.second)) {
        // This could indicate a compromised renderer, since the reporting URLs
        // were already validated in the renderer.
        receiver_.ReportBadMessage(
            base::StrCat({"Invalid reporting URL '",
                          metadata_pair.second.possibly_invalid_spec(),
                          "' for '", metadata_pair.first, "'"}));
        LogSharedStorageWorkletError(
            blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible);
        return;
      }
      reporting_metadata.insert(
          std::make_pair(metadata_pair.first, metadata_pair.second.spec()));
    }

    converted_urls.emplace_back(url_with_metadata->url,
                                std::move(reporting_metadata));
  }

  if (context_id.has_value() &&
      !blink::IsValidPrivateAggregationContextId(context_id.value())) {
    receiver_.ReportBadMessage("Invalid context_id.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible);
    return;
  }

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageSelectURLDisabledMessage,
        /*result_config=*/absl::nullopt);
    return;
  }

  if (!keep_alive_worklet_after_operation_) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageWorkletExpiredMessage,
        /*result_config=*/absl::nullopt);
    return;
  }

  keep_alive_worklet_after_operation_ = keep_alive_after_operation;

  size_t shared_storage_fenced_frame_root_count = 0u;
  size_t fenced_frame_depth =
      static_cast<RenderFrameHostImpl&>(render_frame_host())
          .frame_tree_node()
          ->GetFencedFrameDepth(shared_storage_fenced_frame_root_count);

  DCHECK_LE(shared_storage_fenced_frame_root_count, fenced_frame_depth);

  size_t max_allowed_fenced_frame_depth = base::checked_cast<size_t>(
      blink::features::kSharedStorageMaxAllowedFencedFrameDepthForSelectURL
          .Get());

  if (fenced_frame_depth > max_allowed_fenced_frame_depth) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/
        base::StrCat(
            {"selectURL() is called in a context with a fenced frame depth (",
             base::NumberToString(fenced_frame_depth),
             ") exceeding the maximum allowed number (",
             base::NumberToString(max_allowed_fenced_frame_depth), ")."}),
        /*result_config=*/absl::nullopt);
    return;
  }

  GetSharedStorageWorkletHostManager()->NotifySharedStorageAccessed(
      AccessType::kDocumentSelectURL, main_frame_id(),
      SerializeLastCommittedOrigin(),
      SharedStorageEventParams::CreateForSelectURL(name, serialized_data,
                                                   std::move(converted_urls)));

  GetSharedStorageWorkletHost()->RunURLSelectionOperationOnWorklet(
      name, std::move(urls_with_metadata), std::move(serialized_data),
      keep_alive_after_operation, context_id, std::move(callback));
}

void SharedStorageDocumentServiceImpl::SharedStorageSet(
    const std::u16string& key,
    const std::u16string& value,
    bool ignore_if_present,
    SharedStorageSetCallback callback) {
  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageDisabledMessage);
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
  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageDisabledMessage);
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
  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(/*success=*/false,
                            /*error_message=*/kSharedStorageDisabledMessage);
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
  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(/*success=*/false,
                            /*error_message=*/kSharedStorageDisabledMessage);
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

// static
bool& SharedStorageDocumentServiceImpl::GetBypassIsSharedStorageAllowed() {
  static bool should_bypass = false;
  return should_bypass;
}

SharedStorageDocumentServiceImpl::SharedStorageDocumentServiceImpl(
    RenderFrameHost* rfh)
    : DocumentUserData<SharedStorageDocumentServiceImpl>(rfh),
      main_frame_origin_(
          rfh->GetOutermostMainFrame()->GetLastCommittedOrigin()),
      main_frame_id_(
          static_cast<RenderFrameHostImpl*>(rfh->GetOutermostMainFrame())
              ->devtools_frame_token()
              .ToString()) {}

SharedStorageWorkletHost*
SharedStorageDocumentServiceImpl::GetSharedStorageWorkletHost() {
  return static_cast<StoragePartitionImpl*>(
             render_frame_host().GetProcess()->GetStoragePartition())
      ->GetSharedStorageWorkletHostManager()
      ->GetOrCreateSharedStorageWorkletHost(this);
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

bool SharedStorageDocumentServiceImpl::IsSharedStorageAllowed() {
  if (GetBypassIsSharedStorageAllowed())
    return true;

  // Will trigger a call to
  // `content_settings::PageSpecificContentSettings::BrowsingDataAccessed()` for
  // reporting purposes.
  return GetContentClient()->browser()->IsSharedStorageAllowed(
      render_frame_host().GetBrowserContext(), &render_frame_host(),
      main_frame_origin_, render_frame_host().GetLastCommittedOrigin());
}

bool SharedStorageDocumentServiceImpl::IsSharedStorageSelectURLAllowed() {
  if (GetBypassIsSharedStorageAllowed()) {
    return true;
  }

  // Will trigger a call to
  // `content_settings::PageSpecificContentSettings::BrowsingDataAccessed()` for
  // reporting purposes.
  if (!IsSharedStorageAllowed()) {
    return false;
  }

  return GetContentClient()->browser()->IsSharedStorageSelectURLAllowed(
      render_frame_host().GetBrowserContext(), main_frame_origin_,
      render_frame_host().GetLastCommittedOrigin());
}

bool SharedStorageDocumentServiceImpl::IsSharedStorageAddModuleAllowed() {
  if (GetBypassIsSharedStorageAllowed()) {
    return true;
  }

  // Will trigger a call to
  // `content_settings::PageSpecificContentSettings::BrowsingDataAccessed()` for
  // reporting purposes.
  if (!IsSharedStorageAllowed()) {
    return false;
  }

  return GetContentClient()->browser()->IsSharedStorageSelectURLAllowed(
             render_frame_host().GetBrowserContext(), main_frame_origin_,
             render_frame_host().GetLastCommittedOrigin()) ||
         GetContentClient()->browser()->IsPrivateAggregationAllowed(
             render_frame_host().GetBrowserContext(), main_frame_origin_,
             render_frame_host().GetLastCommittedOrigin());
}

std::string SharedStorageDocumentServiceImpl::SerializeLastCommittedOrigin()
    const {
  return render_frame_host().GetLastCommittedOrigin().Serialize();
}

DOCUMENT_USER_DATA_KEY_IMPL(SharedStorageDocumentServiceImpl);

}  // namespace content
