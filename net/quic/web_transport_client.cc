// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/web_transport_client.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "net/quic/dedicated_web_transport_http3_client.h"

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
  void Connect() override { visitor_->OnConnectionFailed(error_); }
  void Close(const std::optional<WebTransportCloseInfo>& close_info) override {
    NOTREACHED_IN_MIGRATION();
  }

  quic::WebTransportSession* session() override { return nullptr; }

 private:
  WebTransportError error_;
  raw_ptr<WebTransportClientVisitor> visitor_;
};
}  // namespace

std::ostream& operator<<(std::ostream& os, WebTransportState state) {
  os << WebTransportStateString(state);
  return os;
}

const char* WebTransportStateString(WebTransportState state) {
  switch (state) {
    case WebTransportState::NEW:
      return "NEW";
    case WebTransportState::CONNECTING:
      return "CONNECTING";
    case WebTransportState::CONNECTED:
      return "CONNECTED";
    case WebTransportState::CLOSED:
      return "CLOSED";
    case WebTransportState::FAILED:
      return "FAILED";
    case WebTransportState::NUM_STATES:
      return "UNKNOWN";
  }
}

WebTransportCloseInfo::WebTransportCloseInfo() = default;
WebTransportCloseInfo::WebTransportCloseInfo(uint32_t code,
                                             std::string_view reason)
    : code(code), reason(reason) {}
WebTransportCloseInfo::~WebTransportCloseInfo() = default;
bool WebTransportCloseInfo::operator==(
    const WebTransportCloseInfo& other) const {
  return code == other.code && reason == other.reason;
}

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
    const NetworkAnonymizationKey& anonymization_key,
    URLRequestContext* context,
    const WebTransportParameters& parameters) {
  if (url.scheme() == url::kHttpsScheme) {
    if (!parameters.enable_web_transport_http3) {
      return std::make_unique<FailedWebTransportClient>(
          ERR_DISALLOWED_URL_SCHEME, visitor);
    }
    return std::make_unique<DedicatedWebTransportHttp3Client>(
        url, origin, visitor, anonymization_key, context, parameters);
  }

  return std::make_unique<FailedWebTransportClient>(ERR_UNKNOWN_URL_SCHEME,
                                                    visitor);
}

}  // namespace net
