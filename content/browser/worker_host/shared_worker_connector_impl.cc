// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_connector_impl.h"

#include "base/memory/ptr_util.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"

namespace content {

// static
void SharedWorkerConnectorImpl::Create(
    int client_process_id,
    int frame_id,
    mojo::PendingReceiver<blink::mojom::SharedWorkerConnector> receiver) {
  mojo::MakeSelfOwnedReceiver(base::WrapUnique(new SharedWorkerConnectorImpl(
                                  client_process_id, frame_id)),
                              std::move(receiver));
}

SharedWorkerConnectorImpl::SharedWorkerConnectorImpl(int client_process_id,
                                                     int frame_id)
    : client_process_id_(client_process_id), frame_id_(frame_id) {}

void SharedWorkerConnectorImpl::Connect(
    blink::mojom::SharedWorkerInfoPtr info,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    mojo::ScopedMessagePipeHandle message_port,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token) {
  RenderProcessHost* host = RenderProcessHost::FromID(client_process_id_);
  // The render process was already terminated.
  if (!host) {
    mojo::Remote<blink::mojom::SharedWorkerClient> remote_client(
        std::move(client));
    remote_client->OnScriptLoadFailed();
    return;
  }
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (blob_url_token) {
    if (!info->url.SchemeIsBlob()) {
      mojo::ReportBadMessage("SWCI_BLOB_URL_TOKEN_FOR_NON_BLOB_URL");
      return;
    }
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            host->GetBrowserContext(), std::move(blob_url_token));
  }
  SharedWorkerServiceImpl* service =
      static_cast<StoragePartitionImpl*>(host->GetStoragePartition())
          ->GetSharedWorkerService();
  service->ConnectToWorker(client_process_id_, frame_id_, std::move(info),
                           std::move(outside_fetch_client_settings_object),
                           std::move(client), creation_context_type,
                           blink::MessagePortChannel(std::move(message_port)),
                           std::move(blob_url_loader_factory));
}

}  // namespace content
