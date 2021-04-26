// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/web_transport_client.h"

#include "net/quic/dedicated_web_transport_http3_client.h"
#include "net/quic/quic_transport_client.h"

namespace net {

namespace {
// A WebTransport client that starts out in an error state.
class FailedWebTransportClient : public WebTransportClient {
 public:
  explicit FailedWebTransportClient(int net_error,
                                    WebTransportClientVisitor* visitor)
      : error_(net_error,
               quic::QUIC_NO_ERROR,
               ErrorToString(net_error),
               /*safe_to_report_details=*/true),
        visitor_(visitor) {}
  void Connect() override { visitor_->OnConnectionFailed(); }

  quic::WebTransportSession* session() override { return nullptr; }
  const QuicTransportError& error() const override { return error_; }

 private:
  QuicTransportError error_;
  WebTransportClientVisitor* visitor_;
};
}  // namespace

WebTransportClientVisitor::~WebTransportClientVisitor() = default;

WebTransportParameters::WebTransportParameters() = default;
WebTransportParameters::~WebTransportParameters() = default;
WebTransportParameters::WebTransportParameters(const WebTransportParameters&) =
    default;
WebTransportParameters::WebTransportParameters(WebTransportParameters&&) =
    default;

std::unique_ptr<WebTransportClient> CreateWebTransportClient(
    const GURL& url,
    const url::Origin& origin,
    WebTransportClientVisitor* visitor,
    const NetworkIsolationKey& isolation_key,
    URLRequestContext* context,
    const WebTransportParameters& parameters) {
  if (url.scheme() == url::kQuicTransportScheme) {
    if (!parameters.enable_quic_transport) {
      return std::make_unique<FailedWebTransportClient>(
          ERR_DISALLOWED_URL_SCHEME, visitor);
    }
    return std::make_unique<QuicTransportClient>(
        url, origin, visitor, isolation_key, context, parameters);
  }
  if (url.scheme() == url::kHttpsScheme) {
    if (!parameters.enable_web_transport_http3) {
      return std::make_unique<FailedWebTransportClient>(
          ERR_DISALLOWED_URL_SCHEME, visitor);
    }
    return std::make_unique<DedicatedWebTransportHttp3Client>(
        url, origin, visitor, isolation_key, context, parameters);
  }

  return std::make_unique<FailedWebTransportClient>(ERR_UNKNOWN_URL_SCHEME,
                                                    visitor);
}

}  // namespace net
