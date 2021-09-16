// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webtransport/web_transport_connector_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

using network::mojom::WebTransportHandshakeClient;

class InterceptingHandshakeClient final : public WebTransportHandshakeClient {
 public:
  InterceptingHandshakeClient(
      base::WeakPtr<RenderFrameHostImpl> frame,
      const GURL& url,
      mojo::PendingRemote<WebTransportHandshakeClient> remote)
      : frame_(std::move(frame)), url_(url), remote_(std::move(remote)) {}
  ~InterceptingHandshakeClient() override = default;

  // WebTransportHandshakeClient implementation:
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::WebTransport> transport,
      mojo::PendingReceiver<network::mojom::WebTransportClient> client,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers)
      override {
    // We don't need to pass headers to the renderer here.
    remote_->OnConnectionEstablished(
        std::move(transport), std::move(client),
        base::MakeRefCounted<net::HttpResponseHeaders>(
            /*raw_headers=*/""));
  }
  void OnHandshakeFailed(
      const absl::optional<net::WebTransportError>& error) override {
    // Here we pass null because it is dangerous to pass the error details
    // to the initiator renderer.
    remote_->OnHandshakeFailed(absl::nullopt);

    if (RenderFrameHostImpl* frame = frame_.get()) {
      devtools_instrumentation::OnWebTransportHandshakeFailed(frame, url_,
                                                              error);
    }
  }

 private:
  const base::WeakPtr<RenderFrameHostImpl> frame_;
  const GURL url_;
  mojo::Remote<WebTransportHandshakeClient> remote_;
};

}  // namespace

WebTransportConnectorImpl::WebTransportConnectorImpl(
    int process_id,
    base::WeakPtr<RenderFrameHostImpl> frame,
    const url::Origin& origin,
    const net::NetworkIsolationKey& network_isolation_key)
    : process_id_(process_id),
      frame_(std::move(frame)),
      origin_(origin),
      network_isolation_key_(network_isolation_key) {}

WebTransportConnectorImpl::~WebTransportConnectorImpl() = default;

void WebTransportConnectorImpl::Connect(
    const GURL& url,
    std::vector<network::mojom::WebTransportCertificateFingerprintPtr>
        fingerprints,
    mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
        handshake_client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  if (!process) {
    return;
  }

  mojo::PendingRemote<WebTransportHandshakeClient> handshake_client_to_pass;
  // TODO(yhirano): Stop using MakeSelfOwnedReceiver here, because the
  // WebTransport implementation in the network service won't notice that
  // the WebTransportHandshakeClient is going away.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<InterceptingHandshakeClient>(
          frame_, url, std::move(handshake_client)),
      handshake_client_to_pass.InitWithNewPipeAndPassReceiver());

  GetContentClient()->browser()->WillCreateWebTransport(
      frame_.get(), url, std::move(handshake_client_to_pass),
      base::BindOnce(
          &WebTransportConnectorImpl::OnWillCreateWebTransportCompleted,
          weak_factory_.GetWeakPtr(), url, std::move(fingerprints)));
}

void WebTransportConnectorImpl::OnWillCreateWebTransportCompleted(
    const GURL& url,
    std::vector<network::mojom::WebTransportCertificateFingerprintPtr>
        fingerprints,
    mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
        handshake_client,
    absl::optional<network::mojom::WebTransportErrorPtr> error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  if (!process) {
    return;
  }

  if (error) {
    mojo::Remote<WebTransportHandshakeClient> remote(
        std::move(handshake_client));
    remote->OnHandshakeFailed(net::WebTransportError(
        error.value()->net_error,
        static_cast<quic::QuicErrorCode>(error.value()->quic_error),
        error.value()->details, error.value()->safe_to_report_details));
    return;
  }

  process->GetStoragePartition()->GetNetworkContext()->CreateWebTransport(
      url, origin_, network_isolation_key_, std::move(fingerprints),
      std::move(handshake_client));
}

}  // namespace content
