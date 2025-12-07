// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_ALTERNATIVE_SERVICE_H_
#define NET_HTTP_ALTERNATIVE_SERVICE_H_

#include <stdint.h>

#include <algorithm>
#include <ostream>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/alternate_protocol_usage.h"
#include "net/quic/quic_http_utils.h"
#include "net/socket/next_proto.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

// Log a histogram to reflect |usage|.
NET_EXPORT void HistogramAlternateProtocolUsage(AlternateProtocolUsage usage,
                                                bool is_google_host);

enum BrokenAlternateProtocolLocation {
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_JOB = 0,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_QUIC_SESSION_POOL = 1,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_JOB_ALT = 2,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_JOB_MAIN = 3,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_QUIC_HTTP_STREAM = 4,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_NETWORK_TRANSACTION = 5,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_MAX,
};

// Log a histogram to reflect |location|.
NET_EXPORT void HistogramBrokenAlternateProtocolLocation(
    BrokenAlternateProtocolLocation location);

// Returns true if |protocol| is a valid protocol.
NET_EXPORT bool IsAlternateProtocolValid(NextProto protocol);

// Returns true if |protocol| is enabled, based on |is_http2_enabled|
// and |is_quic_enabled|..
NET_EXPORT bool IsProtocolEnabled(NextProto protocol,
                                  bool is_http2_enabled,
                                  bool is_quic_enabled);

// (protocol, host, port) triple as defined in
// https://tools.ietf.org/id/draft-ietf-httpbis-alt-svc-06.html
struct NET_EXPORT AlternativeService {
  AlternativeService() = default;

  AlternativeService(NextProto protocol, std::string_view host, uint16_t port);

  AlternativeService(NextProto protocol, const HostPortPair& host_port_pair);

  AlternativeService(const AlternativeService& alternative_service);
  AlternativeService(AlternativeService&& alternative_service) noexcept;

  AlternativeService& operator=(AlternativeService&& alternative_service);
  AlternativeService& operator=(const AlternativeService& alternative_service);

  HostPortPair GetHostPortPair() const;

  bool operator==(const AlternativeService& other) const = default;

  std::strong_ordering operator<=>(const AlternativeService& other) const;

  // Output format: "protocol host:port", e.g. "h2 www.google.com:1234".
  std::string ToString() const;

  NextProto protocol = NextProto::kProtoUnknown;
  std::string host;
  uint16_t port;
};

NET_EXPORT_PRIVATE std::ostream& operator<<(
    std::ostream& os,
    const AlternativeService& alternative_service);

class NET_EXPORT_PRIVATE AlternativeServiceInfo {
 public:
  static AlternativeServiceInfo CreateHttp2AlternativeServiceInfo(
      const AlternativeService& alternative_service,
      base::Time expiration);

  static AlternativeServiceInfo CreateQuicAlternativeServiceInfo(
      const AlternativeService& alternative_service,
      base::Time expiration,
      const quic::ParsedQuicVersionVector& advertised_versions);

  AlternativeServiceInfo();

  AlternativeServiceInfo(
      const AlternativeServiceInfo& alternative_service_info);
  AlternativeServiceInfo(
      AlternativeServiceInfo&& alternative_service_info) noexcept;

  AlternativeServiceInfo& operator=(
      const AlternativeServiceInfo& alternative_service_info);
  AlternativeServiceInfo& operator=(
      AlternativeServiceInfo&& alternative_service_info);

  ~AlternativeServiceInfo();

  bool operator==(const AlternativeServiceInfo& other) const;

  std::string ToString() const;

  void set_alternative_service(const AlternativeService& alternative_service) {
    alternative_service_ = alternative_service;
  }

  void set_protocol(const NextProto& protocol) {
    alternative_service_.protocol = protocol;
  }

  void set_host(const std::string& host) { alternative_service_.host = host; }

  void set_port(uint16_t port) { alternative_service_.port = port; }

  void set_expiration(base::Time expiration) { expiration_ = expiration; }

  // Sets the advertised versions for QUIC alternative services to a sorted copy
  // of `advertised_versions`.
  void SetAdvertisedVersions(
      const quic::ParsedQuicVersionVector& advertised_versions);

  const AlternativeService& alternative_service() const {
    return alternative_service_;
  }

  NextProto protocol() const { return alternative_service_.protocol; }

  HostPortPair GetHostPortPair() const {
    return alternative_service_.GetHostPortPair();
  }

  base::Time expiration() const { return expiration_; }

  const quic::ParsedQuicVersionVector& advertised_versions() const {
    return advertised_versions_;
  }

 private:
  AlternativeServiceInfo(
      const AlternativeService& alternative_service,
      base::Time expiration,
      const quic::ParsedQuicVersionVector& advertised_versions);

  AlternativeService alternative_service_;
  base::Time expiration_;

  // Lists all the QUIC versions that are advertised by the server and supported
  // by Chrome. If empty, defaults to versions used by the current instance of
  // the netstack. This list is sorted according to the server's preference.
  quic::ParsedQuicVersionVector advertised_versions_;
};

using AlternativeServiceInfoVector = std::vector<AlternativeServiceInfo>;

NET_EXPORT_PRIVATE AlternativeServiceInfoVector ProcessAlternativeServices(
    const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector&
        alternative_service_vector,
    bool is_http2_enabled,
    bool is_quic_enabled,
    const quic::ParsedQuicVersionVector& supported_quic_versions);

}  // namespace net

#endif  // NET_HTTP_ALTERNATIVE_SERVICE_H_
