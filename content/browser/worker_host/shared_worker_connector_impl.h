// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONNECTOR_IMPL_H_

#include <optional>

#include "base/memory/self_deleting.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/worker/shared_worker_connector.mojom.h"

namespace content {

class RenderFrameHost;
class StorageAccessHandle;

// Instances of this class live on the UI thread and have their lifetime scoped
// to the current document of the RenderFrameHost via DocumentService.
class CONTENT_EXPORT SharedWorkerConnectorImpl final
    : public DocumentService<blink::mojom::SharedWorkerConnector> {
 public:
  // Creates and binds a SharedWorkerConnector for the default storage partition
  // of `client_render_frame_host`'s current document.
  static void Create(
      RenderFrameHost& client_render_frame_host,
      mojo::PendingReceiver<blink::mojom::SharedWorkerConnector> receiver);

  // Like the above, but allows overriding of the storage partition used for the
  // created/bound shared worker pool using `storage_key_override`.
  static void Create(
      base::PassKey<StorageAccessHandle>,
      RenderFrameHost& client_render_frame_host,
      const blink::StorageKey& storage_key_override,
      mojo::PendingReceiver<blink::mojom::SharedWorkerConnector> receiver);

  SharedWorkerConnectorImpl(
      RenderFrameHost& client_render_frame_host,
      base::optional_ref<const blink::StorageKey> storage_key_override,
      mojo::PendingReceiver<blink::mojom::SharedWorkerConnector> receiver,
      base::SelfDeletingPassKey key);

  SharedWorkerConnectorImpl(const SharedWorkerConnectorImpl&) = delete;
  SharedWorkerConnectorImpl& operator=(const SharedWorkerConnectorImpl&) =
      delete;

 private:
  // `this` can only be destroyed by DocumentService.
  ~SharedWorkerConnectorImpl() override;

  // blink::mojom::SharedWorkerConnector methods:
  void Connect(
      blink::mojom::SharedWorkerInfoPtr info,
      mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
      blink::mojom::SharedWorkerCreationContextType creation_context_type,
      blink::MessagePortDescriptor message_port,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token) override;

  // Used to force a first-party StorageKey when needed by StorageAccessHandle.
  const std::optional<blink::StorageKey> storage_key_override_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONNECTOR_IMPL_H_
