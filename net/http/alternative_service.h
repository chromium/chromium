// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_ALTERNATIVE_SERVICE_H_
#define NET_HTTP_ALTERNATIVE_SERVICE_H_

#include <stdint.h>

#include <algorithm>
#include <ostream>
#include <string>

#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/port_util.h"
#include "net/quic/quic_http_utils.h"
#include "net/socket/next_proto.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"

namespace net {

enum AlternateProtocolUsage {
  // Alternate Protocol was used without racing a normal connection.
  ALTERNATE_PROTOCOL_USAGE_NO_RACE = 0,
  // Alternate Protocol was used by winning a race with a normal connection.
  ALTERNATE_PROTOCOL_USAGE_WON_RACE = 1,
  // Alternate Protocol was not used by losing a race with a normal connection.
  ALTERNATE_PROTOCOL_USAGE_LOST_RACE = 2,
  // Alternate Protocol was not used because no Alternate-Protocol information
  // was available when the request was issued, but an Alternate-Protocol header
  // was present in the response.
  ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING = 3,
  // Alternate Protocol was not used because it was marked broken.
  ALTERNATE_PROTOCOL_USAGE_BROKEN = 4,
  // Maximum value for the enum.
  ALTERNATE_PROTOCOL_USAGE_MAX,
};

// Log a histogram to reflect |usage|.
NET_EXPORT void HistogramAlternateProtocolUsage(AlternateProtocolUsage usage,
                                                bool proxy_server_used);

enum BrokenAlternateProtocolLocation {
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_JOB = 0,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_QUIC_STREAM_FACTORY = 1,
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
//
// TODO(mmenke):  Seems like most of this stuff should be de-inlined.
struct NET_EXPORT AlternativeService {
  AlternativeService() : protocol(kProtoUnknown), host(), port(0) {}

  AlternativeService(NextProto protocol, const std::string& host, uint16_t port)
      : protocol(protocol), host(host), port(port) {}

  AlternativeService(NextProto protocol, const HostPortPair& host_port_pair)
      : protocol(protocol),
        host(host_port_pair.host()),
        port(host_port_pair.port()) {}

  AlternativeService(const AlternativeService& alternative_service) = default;
  AlternativeService& operator=(const AlternativeService& alternative_service) =
      default;

  HostPortPair host_port_pair() const { return HostPortPair(host, port); }

  bool operator==(const AlternativeService& other) const {
    return protocol == other.protocol && host == other.host &&
           port == other.port;
  }

  bool operator!=(const AlternativeService& other) const {
    return !this->operator==(other);
  }

  bool operator<(const AlternativeService& other) const {
    return std::tie(protocol, host, port) <
           std::tie(other.protocol, other.host, other.port);
  }

  // Output format: "protocol host:port", e.g. "h2 www.google.com:1234".
  std::string ToString() const;

  NextProto protocol;
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
  ~AlternativeServiceInfo();

  AlternativeServiceInfo(
      const AlternativeServiceInfo& alternative_service_info);

  AlternativeServiceInfo& operator=(
      const AlternativeServiceInfo& alternative_service_info);

  bool operator==(const AlternativeServiceInfo& other) const {
    return alternative_service_ == other.alternative_service() &&
           expiration_ == other.expiration() &&
           advertised_versions_ == other.advertised_versions();
  }

  bool operator!=(const AlternativeServiceInfo& other) const {
    return !this->operator==(other);
  }

  std::string ToString() const;

  void set_alternative_service(const AlternativeService& alternative_service) {
    alternative_service_ = alternative_service;
  }

  void set_protocol(const NextProto& protocol) {
    alternative_service_.protocol = protocol;
  }

  void set_host(const std::string& host) { alternative_service_.host = host; }

  void set_port(uint16_t port) { alternative_service_.port = port; }

  void set_expiration(const base::Time& expiration) {
    expiration_ = expiration;
  }

  void set_advertised_versions(
      const quic::ParsedQuicVersionVector& advertised_versions) {
    if (alternative_service_.protocol != kProtoQUIC)
      return;

    advertised_versions_ = advertised_versions;
    std::sort(advertised_versions_.begin(), advertised_versions_.end(),
              TransportVersionLessThan);
  }

  const AlternativeService& alternative_service() const {
    return alternative_service_;
  }

  NextProto protocol() const { return alternative_service_.protocol; }

  HostPortPair host_port_pair() const {
    return alternative_service_.host_port_pair();
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

  static bool TransportVersionLessThan(const quic::ParsedQuicVersion& lhs,
                                       const quic::ParsedQuicVersion& rhs);

  AlternativeService alternative_service_;
  base::Time expiration_;

  // Lists all the QUIC versions that are advertised by the server and supported
  // by Chrome. If empty, defaults to versions used by the current instance of
  // the netstack.
  // This list MUST be sorted in ascending order.
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
