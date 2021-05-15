// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_WEB_TRANSPORT_CLIENT_H_
#define NET_QUIC_WEB_TRANSPORT_CLIENT_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "net/base/network_isolation_key.h"
#include "net/quic/web_transport_error.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/core/web_transport_interface.h"
#include "net/third_party/quiche/src/quic/quic_transport/web_transport_fingerprint_proof_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class URLRequestContext;

// Diagram of allowed state transitions:
//
//    NEW -> CONNECTING -> CONNECTED -> CLOSED
//              |                |
//              |                |
//              +---> FAILED <---+
//
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "QuicTransportClientState" in src/tools/metrics/histograms/enums.xml.
enum WebTransportState {
  // The client object has been created but Connect() has not been called.
  NEW,
  // Connection establishment is in progress.  No application data can be sent
  // or received at this point.
  CONNECTING,
  // The connection has been established and application data can be sent and
  // received.
  CONNECTED,
  // The connection has been closed gracefully by either endpoint.
  CLOSED,
  // The connection has been closed abruptly.
  FAILED,

  // Total number of possible states.
  NUM_STATES,
};

// A visitor that gets notified about events that happen to a WebTransport
// client.
class NET_EXPORT WebTransportClientVisitor {
 public:
  virtual ~WebTransportClientVisitor();

  // State change notifiers.
  virtual void OnConnected() = 0;         // CONNECTING -> CONNECTED
  virtual void OnConnectionFailed() = 0;  // CONNECTING -> FAILED
  virtual void OnClosed() = 0;            // CONNECTED -> CLOSED
  virtual void OnError() = 0;             // CONNECTED -> FAILED

  virtual void OnIncomingBidirectionalStreamAvailable() = 0;
  virtual void OnIncomingUnidirectionalStreamAvailable() = 0;
  virtual void OnDatagramReceived(base::StringPiece datagram) = 0;
  virtual void OnCanCreateNewOutgoingBidirectionalStream() = 0;
  virtual void OnCanCreateNewOutgoingUnidirectionalStream() = 0;
  virtual void OnDatagramProcessed(
      absl::optional<quic::MessageStatus> status) = 0;
};

// Parameters that determine the way WebTransport session is established.
struct NET_EXPORT WebTransportParameters {
  WebTransportParameters();
  ~WebTransportParameters();
  WebTransportParameters(const WebTransportParameters&);
  WebTransportParameters(WebTransportParameters&&);

  bool allow_pooling = false;

  bool enable_quic_transport = false;
  bool enable_web_transport_http3 = false;

  // A vector of fingerprints for expected server certificates, as described in
  // https://wicg.github.io/web-transport/#dom-quictransportconfiguration-server_certificate_fingerprints
  // When empty, Web PKI is used.
  std::vector<quic::CertificateFingerprint> server_certificate_fingerprints;
};

// An abstract base for a WebTransport client.  Most of the useful operations
// are available via the underlying WebTransportSession object, that can be
// accessed through the session() method.
class NET_EXPORT WebTransportClient {
 public:
  virtual ~WebTransportClient() = default;

  // Connect() is an asynchronous operation.  Once the operation is finished,
  // OnConnected() or OnConnectionFailed() is called on the Visitor.
  virtual void Connect() = 0;

  // session() can be nullptr in states other than CONNECTED.
  virtual quic::WebTransportSession* session() = 0;
  virtual const WebTransportError& error() const = 0;
};

// Creates a WebTransport client for |url| accessed from |origin| with the
// provided |isolation_key|; |visitor| is associated with the resulting object.
// This method never returns nullptr; in case of error, the resulting client
// will be in the error state.
NET_EXPORT
std::unique_ptr<WebTransportClient> CreateWebTransportClient(
    const GURL& url,
    const url::Origin& origin,
    WebTransportClientVisitor* visitor,
    const NetworkIsolationKey& isolation_key,
    URLRequestContext* context,
    const WebTransportParameters& parameters);

}  // namespace net

#endif  // NET_QUIC_WEB_TRANSPORT_CLIENT_H_
