// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONNECTOR_IMPL_H_

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/mojom/worker/shared_worker_connector.mojom.h"

namespace content {

// Instances of this class live on the UI thread and have their lifetime bound
// to a Mojo connection.
class CONTENT_EXPORT SharedWorkerConnectorImpl
    : public blink::mojom::SharedWorkerConnector {
 public:
  static void Create(
      GlobalRenderFrameHostId client_render_frame_host_id,
      mojo::PendingReceiver<blink::mojom::SharedWorkerConnector> receiver);

 private:
  explicit SharedWorkerConnectorImpl(
      GlobalRenderFrameHostId client_render_frame_host_id);

  // blink::mojom::SharedWorkerConnector methods:
  void Connect(
      blink::mojom::SharedWorkerInfoPtr info,
      mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
      blink::mojom::SharedWorkerCreationContextType creation_context_type,
      blink::MessagePortDescriptor message_port,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      ukm::SourceId client_ukm_source_id) override;

  const GlobalRenderFrameHostId client_render_frame_host_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONNECTOR_IMPL_H_
