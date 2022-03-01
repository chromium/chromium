// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_document_service_impl.h"

#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"

namespace content {

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
    const std::vector<uint8_t>& serialized_data) {
  GetSharedStorageWorkletHost()->RunOperationOnWorklet(name, serialized_data);
}

void SharedStorageDocumentServiceImpl::RunURLSelectionOperationOnWorklet(
    const std::string& name,
    const std::vector<GURL>& urls,
    const std::vector<uint8_t>& serialized_data,
    RunURLSelectionOperationOnWorkletCallback callback) {
  if (!blink::IsValidSharedStorageURLsArrayLength(urls.size())) {
    // This could indicate a compromised renderer, so let's terminate it.
    receiver_.ReportBadMessage(
        "Attempted to execute RunURLSelectionOperationOnWorklet with invalid "
        "URLs array length.");
    return;
  }

  GetSharedStorageWorkletHost()->RunURLSelectionOperationOnWorklet(
      name, urls, serialized_data, std::move(callback));
}

base::WeakPtr<SharedStorageDocumentServiceImpl>
SharedStorageDocumentServiceImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

SharedStorageDocumentServiceImpl::SharedStorageDocumentServiceImpl(
    RenderFrameHost* rfh)
    : DocumentUserData<SharedStorageDocumentServiceImpl>(rfh) {}

SharedStorageWorkletHost*
SharedStorageDocumentServiceImpl::GetSharedStorageWorkletHost() {
  return static_cast<StoragePartitionImpl*>(
             render_frame_host().GetProcess()->GetStoragePartition())
      ->GetSharedStorageWorkletHostManager()
      ->GetOrCreateSharedStorageWorkletHost(this);
}

DOCUMENT_USER_DATA_KEY_IMPL(SharedStorageDocumentServiceImpl);

}  // namespace content
