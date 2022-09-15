// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_document_service_impl.h"

#include "base/strings/strcat.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "url/url_constants.h"

namespace content {

namespace {

// TODO(crbug.com/1335504): Consider moving this function to
// third_party/blink/common/fenced_frame/fenced_frame_utils.cc.
bool IsValidFencedFrameReportingURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  return url.SchemeIs(url::kHttpsScheme);
}

}  // namespace

const char kSharedStorageDisabledMessage[] = "sharedStorage is disabled";

// static
bool& SharedStorageDocumentServiceImpl::
    GetBypassIsSharedStorageAllowedForTesting() {
  return GetBypassIsSharedStorageAllowed();
}

SharedStorageDocumentServiceImpl::~SharedStorageDocumentServiceImpl() {
  static_cast<StoragePartitionImpl*>(
      render_frame_host().GetProcess()->GetStoragePartition())
      ->GetSharedStorageWorkletHostManager()
      ->OnDocumentServiceDestroyed(this);
}

void SharedStorageDocumentServiceImpl::Bind(
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageDocumentService>
        receiver) {
  CHECK(!receiver_)
      << "Multiple attempts to bind the SharedStorageDocumentService receiver";

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

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(/*success=*/false,
                            /*error_message=*/kSharedStorageDisabledMessage);
    return;
  }

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
    const std::vector<uint8_t>& serialized_data,
    RunOperationOnWorkletCallback callback) {
  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(/*success=*/false,
                            /*error_message=*/kSharedStorageDisabledMessage);
    return;
  }

  GetSharedStorageWorkletHost()->RunOperationOnWorklet(name, serialized_data);
  std::move(callback).Run(/*success=*/true, /*error_message=*/{});
}

void SharedStorageDocumentServiceImpl::RunURLSelectionOperationOnWorklet(
    const std::string& name,
    std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
        urls_with_metadata,
    const std::vector<uint8_t>& serialized_data,
    RunURLSelectionOperationOnWorkletCallback callback) {
  if (render_frame_host().IsNestedWithinFencedFrame()) {
    // This could indicate a compromised renderer, so let's terminate it.
    receiver_.ReportBadMessage(
        "Attempted to execute RunURLSelectionOperationOnWorklet within a "
        "fenced frame.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible);
    return;
  }

  if (!blink::IsValidSharedStorageURLsArrayLength(urls_with_metadata.size())) {
    // This could indicate a compromised renderer, so let's terminate it.
    receiver_.ReportBadMessage(
        "Attempted to execute RunURLSelectionOperationOnWorklet with invalid "
        "URLs array length.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible);
    return;
  }

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
    }
  }

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(/*success=*/false,
                            /*error_message=*/kSharedStorageDisabledMessage,
                            GURL());
    return;
  }

  GetSharedStorageWorkletHost()->RunURLSelectionOperationOnWorklet(
      name, std::move(urls_with_metadata), serialized_data,
      std::move(callback));
}

void SharedStorageDocumentServiceImpl::SharedStorageSet(
    const std::u16string& key,
    const std::u16string& value,
    bool ignore_if_present,
    SharedStorageSetCallback callback) {
  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(/*success=*/false,
                            /*error_message=*/kSharedStorageDisabledMessage);
    return;
  }

  storage::SharedStorageDatabase::SetBehavior set_behavior =
      ignore_if_present
          ? storage::SharedStorageDatabase::SetBehavior::kIgnoreIfPresent
          : storage::SharedStorageDatabase::SetBehavior::kDefault;

  GetSharedStorageManager()->Set(render_frame_host().GetLastCommittedOrigin(),
                                 key, value, base::DoNothing(), set_behavior);
  std::move(callback).Run(/*success=*/true, /*error_message=*/{});
}

void SharedStorageDocumentServiceImpl::SharedStorageAppend(
    const std::u16string& key,
    const std::u16string& value,
    SharedStorageAppendCallback callback) {
  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(/*success=*/false,
                            /*error_message=*/kSharedStorageDisabledMessage);
    return;
  }

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
          rfh->GetOutermostMainFrame()->GetLastCommittedOrigin()) {}

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

bool SharedStorageDocumentServiceImpl::IsSharedStorageAllowed() {
  if (GetBypassIsSharedStorageAllowed())
    return true;

  return GetContentClient()->browser()->IsSharedStorageAllowed(
      render_frame_host().GetBrowserContext(), main_frame_origin_,
      render_frame_host().GetLastCommittedOrigin());
}

DOCUMENT_USER_DATA_KEY_IMPL(SharedStorageDocumentServiceImpl);

}  // namespace content
