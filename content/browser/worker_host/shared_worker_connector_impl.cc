// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_connector_impl.h"

#include <optional>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"

namespace content {

// static
void SharedWorkerConnectorImpl::Create(
    RenderFrameHost& client_render_frame_host,
    mojo::PendingReceiver<blink::mojom::SharedWorkerConnector> receiver) {
  base::MakeSelfDeleting<SharedWorkerConnectorImpl>(
      client_render_frame_host, std::nullopt, std::move(receiver));
}

// static
void SharedWorkerConnectorImpl::Create(
    base::PassKey<StorageAccessHandle>,
    RenderFrameHost& client_render_frame_host,
    const blink::StorageKey& storage_key_override,
    mojo::PendingReceiver<blink::mojom::SharedWorkerConnector> receiver) {
  base::MakeSelfDeleting<SharedWorkerConnectorImpl>(
      client_render_frame_host, storage_key_override, std::move(receiver));
}

SharedWorkerConnectorImpl::SharedWorkerConnectorImpl(
    RenderFrameHost& client_render_frame_host,
    base::optional_ref<const blink::StorageKey> storage_key_override,
    mojo::PendingReceiver<blink::mojom::SharedWorkerConnector> receiver,
    base::SelfDeletingPassKey key)
    : DocumentService(client_render_frame_host, std::move(receiver), key),
      storage_key_override_(storage_key_override.CopyAsOptional()) {}

SharedWorkerConnectorImpl::~SharedWorkerConnectorImpl() = default;

void SharedWorkerConnectorImpl::Connect(
    blink::mojom::SharedWorkerInfoPtr info,
    mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    blink::MessagePortDescriptor message_port,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token) {
  StoragePartition* storage_partition =
      render_frame_host().GetStoragePartition();
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (blob_url_token) {
    if (!info->url.SchemeIsBlob()) {
      ReportBadMessageAndDeleteThis("SWCI_BLOB_URL_TOKEN_FOR_NON_BLOB_URL");
      return;
    }
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            storage_partition, std::move(blob_url_token));
  }
  SharedWorkerServiceImpl* service = static_cast<SharedWorkerServiceImpl*>(
      storage_partition->GetSharedWorkerService());
  service->ConnectToWorker(
      render_frame_host().GetGlobalId(), std::move(info), std::move(client),
      creation_context_type, blink::MessagePortChannel(std::move(message_port)),
      std::move(blob_url_loader_factory), storage_key_override_);
}

}  // namespace content
