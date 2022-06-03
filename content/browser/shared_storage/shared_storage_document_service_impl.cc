// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_document_service_impl.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_frame_host.h"

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
          url::Origin::Create(script_source_url))) {
    // This could indicate a compromised renderer, so let's terminate it.
    mojo::ReportBadMessage("Attempted to load a cross-origin module script.");

    // Explicitly close the pipe here so that the `callback` can be safely
    // dropped.
    receiver_.reset();

    return;
  }

  GetSharedStorageWorkletHost()->AddModuleOnWorklet(
      render_frame_host().GetLastCommittedOrigin(), script_source_url,
      std::move(callback));
}

void SharedStorageDocumentServiceImpl::RunOperationOnWorklet(
    const std::string& name,
    const std::vector<uint8_t>& serialized_data) {
  GetSharedStorageWorkletHost()->RunOperationOnWorklet(name, serialized_data);
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
