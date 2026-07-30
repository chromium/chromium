// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_CONNECTOR_IMPL_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "content/browser/webtransport/web_transport_throttle_context.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_anonymization_key.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom.h"
#include "url/origin.h"

namespace network::mojom {
class URLLoaderNetworkServiceObserver;
}  // namespace network::mojom

namespace content {

class RenderFrameHostImpl;

class WebTransportConnectorImpl final
    : public blink::mojom::WebTransportConnector {
 public:
  // |frame| is needed for devtools and the throttle context. For shared or
  // service workers, there is no appropriate frame to associate, and in that
  // case nullptr should be passed.
  WebTransportConnectorImpl(
      int process_id,
      base::WeakPtr<RenderFrameHostImpl> frame,
      WeakDocumentPtr weak_document,
      const url::Origin& origin,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ClientSecurityStatePtr client_security_state,
      const base::UnguessableToken& network_restrictions_id);
  ~WebTransportConnectorImpl() override;

  void Connect(
      const GURL& url,
      std::vector<network::mojom::WebTransportCertificateFingerprintPtr>
          fingerprints,
      const std::vector<std::string>& application_protocols,
      network::mojom::WebTransportCongestionControl congestion_control,
      std::optional<uint16_t>
          anticipated_concurrent_incoming_unidirectional_streams,
      std::optional<uint16_t>
          anticipated_concurrent_incoming_bidirectional_streams,
      std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>
          additional_headers,
      mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
          handshake_client) override;

 private:
  void OnThrottleDone(
      const GURL& url,
      std::vector<network::mojom::WebTransportCertificateFingerprintPtr>
          fingerprints,
      const std::vector<std::string>& application_protocols,
      network::mojom::WebTransportCongestionControl congestion_control,
      std::optional<uint16_t>
          anticipated_concurrent_incoming_unidirectional_streams,
      std::optional<uint16_t>
          anticipated_concurrent_incoming_bidirectional_streams,
      std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>
          additional_headers,
      mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
          handshake_client,
      std::unique_ptr<WebTransportThrottleContext::Tracker> tracker);

  void OnWillCreateWebTransportCompleted(
      const GURL& url,
      std::vector<network::mojom::WebTransportCertificateFingerprintPtr>
          fingerprints,
      const std::vector<std::string>& application_protocols,
      network::mojom::WebTransportCongestionControl congestion_control,
      std::optional<uint16_t>
          anticipated_concurrent_incoming_unidirectional_streams,
      std::optional<uint16_t>
          anticipated_concurrent_incoming_bidirectional_streams,
      std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>
          additional_headers,
      mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      network::mojom::ClientSecurityStatePtr client_security_state,
      mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
          handshake_client,
      std::optional<network::mojom::WebTransportErrorPtr> error);

  const int process_id_;
  const base::WeakPtr<RenderFrameHostImpl> frame_;
  const WeakDocumentPtr weak_document_;
  // Records whether this connector was created for a document-scoped context
  // (RenderFrame or DedicatedWorker) where `weak_document` was valid at
  // construction, as opposed to a document-independent context (SharedWorker
  // or ServiceWorker). We store this separately because checking `frame_` alone
  // is insufficient: during same-site navigations, a RenderFrameHost may be
  // reused (`frame_` remains valid), but its underlying document changes. This
  // flag allows `Connect()` to abort connections if the original document is
  // no longer active, while allowing Shared/Service Worker connections to
  // proceed.
  const bool has_document_;
  const url::Origin origin_;
  const net::NetworkAnonymizationKey network_anonymization_key_;
  const network::mojom::ClientSecurityStatePtr client_security_state_;
  const base::WeakPtr<WebTransportThrottleContext> throttle_context_;
  const base::UnguessableToken network_restrictions_id_;

  base::WeakPtrFactory<WebTransportConnectorImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_CONNECTOR_IMPL_H_
