// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONNECTOR_IMPL_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/worker/shared_worker_connector.mojom.h"

namespace content {

// Instances of this class live on the UI thread and have their lifetime bound
// to a Mojo connection.
class CONTENT_EXPORT SharedWorkerConnectorImpl
    : public blink::mojom::SharedWorkerConnector {
 public:
  static void Create(
      int client_process_id,
      int frame_id,
      mojo::PendingReceiver<blink::mojom::SharedWorkerConnector> receiver);

 private:
  SharedWorkerConnectorImpl(int client_process_id, int frame_id);

  // blink::mojom::SharedWorkerConnector methods:
  void Connect(
      blink::mojom::SharedWorkerInfoPtr info,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
      blink::mojom::SharedWorkerCreationContextType creation_context_type,
      mojo::ScopedMessagePipeHandle message_port,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token) override;

  const int client_process_id_;
  const int frame_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONNECTOR_IMPL_H_
