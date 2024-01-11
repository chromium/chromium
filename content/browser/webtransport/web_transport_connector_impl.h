// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_CONNECTOR_IMPL_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "content/browser/webtransport/web_transport_throttle_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHostImpl;

class WebTransportConnectorImpl final
    : public blink::mojom::WebTransportConnector {
 public:
  // |frame| is needed for devtools and the throttle context. Sometimes (e.g.,
  // the connector is for shared or service workers) there is no appropriate
  // frame to associate, and in that case nullptr should be passed.
  WebTransportConnectorImpl(
      int process_id,
      base::WeakPtr<RenderFrameHostImpl> frame,
      const url::Origin& origin,
      const net::NetworkAnonymizationKey& network_anonymization_key);
  ~WebTransportConnectorImpl() override;

  void Connect(
      const GURL& url,
      std::vector<network::mojom::WebTransportCertificateFingerprintPtr>
          fingerprints,
      mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
          handshake_client) override;

 private:
  void OnThrottleDone(
      const GURL& url,
      std::vector<network::mojom::WebTransportCertificateFingerprintPtr>
          fingerprints,
      mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
          handshake_client,
      std::unique_ptr<WebTransportThrottleContext::Tracker> tracker);

  void OnWillCreateWebTransportCompleted(
      const GURL& url,
      std::vector<network::mojom::WebTransportCertificateFingerprintPtr>
          fingerprints,
      mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
          handshake_client,
      std::optional<network::mojom::WebTransportErrorPtr> error);

  const int process_id_;
  const base::WeakPtr<RenderFrameHostImpl> frame_;
  const url::Origin origin_;
  const net::NetworkAnonymizationKey network_anonymization_key_;
  const base::WeakPtr<WebTransportThrottleContext> throttle_context_;

  base::WeakPtrFactory<WebTransportConnectorImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_CONNECTOR_IMPL_H_
