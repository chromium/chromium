// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/alternative_service.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"

namespace net {

namespace {

enum AlternativeProxyUsage {
  // Alternative Proxy was used without racing a normal connection.
  ALTERNATIVE_PROXY_USAGE_NO_RACE = 0,
  // Alternative Proxy was used by winning a race with a normal connection.
  ALTERNATIVE_PROXY_USAGE_WON_RACE = 1,
  // Alternative Proxy was not used by losing a race with a normal connection.
  ALTERNATIVE_PROXY_USAGE_LOST_RACE = 2,
  // Maximum value for the enum.
  ALTERNATIVE_PROXY_USAGE_MAX,
};

AlternativeProxyUsage ConvertProtocolUsageToProxyUsage(
    AlternateProtocolUsage usage) {
  switch (usage) {
    case ALTERNATE_PROTOCOL_USAGE_NO_RACE:
      return ALTERNATIVE_PROXY_USAGE_NO_RACE;
    case ALTERNATE_PROTOCOL_USAGE_WON_RACE:
      return ALTERNATIVE_PROXY_USAGE_WON_RACE;
    case ALTERNATE_PROTOCOL_USAGE_LOST_RACE:
      return ALTERNATIVE_PROXY_USAGE_LOST_RACE;
    default:
      NOTREACHED();
      return ALTERNATIVE_PROXY_USAGE_MAX;
  }
}

quic::ParsedQuicVersion ParsedQuicVersionFromAlpn(
    base::StringPiece str,
    quic::ParsedQuicVersionVector supported_versions) {
  for (const quic::ParsedQuicVersion version : supported_versions) {
    if (AlpnForVersion(version) == str)
      return version;
  }
  return {quic::PROTOCOL_UNSUPPORTED, quic::QUIC_VERSION_UNSUPPORTED};
}

}  // anonymous namespace

void HistogramAlternateProtocolUsage(AlternateProtocolUsage usage,
                                     bool proxy_server_used) {
  if (proxy_server_used) {
    DCHECK_LE(usage, ALTERNATE_PROTOCOL_USAGE_LOST_RACE);
    UMA_HISTOGRAM_ENUMERATION("Net.QuicAlternativeProxy.Usage",
                              ConvertProtocolUsageToProxyUsage(usage),
                              ALTERNATIVE_PROXY_USAGE_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Net.AlternateProtocolUsage", usage,
                              ALTERNATE_PROTOCOL_USAGE_MAX);
  }
}

void HistogramBrokenAlternateProtocolLocation(
    BrokenAlternateProtocolLocation location) {
  UMA_HISTOGRAM_ENUMERATION("Net.AlternateProtocolBrokenLocation", location,
                            BROKEN_ALTERNATE_PROTOCOL_LOCATION_MAX);
}

bool IsAlternateProtocolValid(NextProto protocol) {
  switch (protocol) {
    case kProtoUnknown:
      return false;
    case kProtoHTTP11:
      return false;
    case kProtoHTTP2:
      return true;
    case kProtoQUIC:
      return true;
  }
  NOTREACHED();
  return false;
}

bool IsProtocolEnabled(NextProto protocol,
                       bool is_http2_enabled,
                       bool is_quic_enabled) {
  switch (protocol) {
    case kProtoUnknown:
      NOTREACHED();
      return false;
    case kProtoHTTP11:
      return true;
    case kProtoHTTP2:
      return is_http2_enabled;
    case kProtoQUIC:
      return is_quic_enabled;
  }
  NOTREACHED();
  return false;
}

// static
AlternativeServiceInfo
AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
    const AlternativeService& alternative_service,
    base::Time expiration) {
  DCHECK_EQ(alternative_service.protocol, kProtoHTTP2);
  return AlternativeServiceInfo(alternative_service, expiration,
                                quic::ParsedQuicVersionVector());
}

// static
AlternativeServiceInfo AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
    const AlternativeService& alternative_service,
    base::Time expiration,
    const quic::ParsedQuicVersionVector& advertised_versions) {
  DCHECK_EQ(alternative_service.protocol, kProtoQUIC);
  return AlternativeServiceInfo(alternative_service, expiration,
                                advertised_versions);
}

AlternativeServiceInfo::AlternativeServiceInfo() : alternative_service_() {}

AlternativeServiceInfo::~AlternativeServiceInfo() = default;

AlternativeServiceInfo::AlternativeServiceInfo(
    const AlternativeService& alternative_service,
    base::Time expiration,
    const quic::ParsedQuicVersionVector& advertised_versions)
    : alternative_service_(alternative_service), expiration_(expiration) {
  if (alternative_service_.protocol == kProtoQUIC) {
    advertised_versions_ = advertised_versions;
    std::sort(advertised_versions_.begin(), advertised_versions_.end(),
              TransportVersionLessThan);
  }
}

AlternativeServiceInfo::AlternativeServiceInfo(
    const AlternativeServiceInfo& alternative_service_info) = default;

AlternativeServiceInfo& AlternativeServiceInfo::operator=(
    const AlternativeServiceInfo& alternative_service_info) = default;

std::string AlternativeService::ToString() const {
  return base::StringPrintf("%s %s:%d", NextProtoToString(protocol),
                            host.c_str(), port);
}

std::string AlternativeServiceInfo::ToString() const {
  base::Time::Exploded exploded;
  expiration_.LocalExplode(&exploded);
  return base::StringPrintf(
      "%s, expires %04d-%02d-%02d %02d:%02d:%02d",
      alternative_service_.ToString().c_str(), exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second);
}

// static
bool AlternativeServiceInfo::TransportVersionLessThan(
    const quic::ParsedQuicVersion& lhs,
    const quic::ParsedQuicVersion& rhs) {
  return lhs.transport_version < rhs.transport_version;
}

std::ostream& operator<<(std::ostream& os,
                         const AlternativeService& alternative_service) {
  os << alternative_service.ToString();
  return os;
}

AlternativeServiceInfoVector ProcessAlternativeServices(
    const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector&
        alternative_service_vector,
    bool is_http2_enabled,
    bool is_quic_enabled,
    const quic::ParsedQuicVersionVector& supported_quic_versions) {
  // Convert spdy::SpdyAltSvcWireFormat::AlternativeService entries
  // to net::AlternativeServiceInfo.
  AlternativeServiceInfoVector alternative_service_info_vector;
  for (const spdy::SpdyAltSvcWireFormat::AlternativeService&
           alternative_service_entry : alternative_service_vector) {
    if (!IsPortValid(alternative_service_entry.port))
      continue;

    NextProto protocol =
        NextProtoFromString(alternative_service_entry.protocol_id);
    // Check if QUIC version is supported. Filter supported QUIC versions.
    quic::ParsedQuicVersionVector advertised_versions;
    if (protocol == kProtoQUIC) {
      if (!IsProtocolEnabled(protocol, is_http2_enabled, is_quic_enabled))
        continue;
      if (!alternative_service_entry.version.empty()) {
        advertised_versions = FilterSupportedAltSvcVersions(
            alternative_service_entry, supported_quic_versions);
        if (advertised_versions.empty())
          continue;
      }
    } else if (!IsAlternateProtocolValid(protocol)) {
      quic::ParsedQuicVersion version = ParsedQuicVersionFromAlpn(
          alternative_service_entry.protocol_id, supported_quic_versions);
      if (version.handshake_protocol == quic::PROTOCOL_UNSUPPORTED ||
          version.transport_version == quic::QUIC_VERSION_UNSUPPORTED) {
        continue;
      }
      protocol = kProtoQUIC;
      advertised_versions = {version};
    }
    if (!IsAlternateProtocolValid(protocol) ||
        !IsProtocolEnabled(protocol, is_http2_enabled, is_quic_enabled)) {
      continue;
    }

    AlternativeService alternative_service(protocol,
                                           alternative_service_entry.host,
                                           alternative_service_entry.port);
    base::Time expiration =
        base::Time::Now() +
        base::TimeDelta::FromSeconds(alternative_service_entry.max_age);
    AlternativeServiceInfo alternative_service_info;
    if (protocol == kProtoQUIC) {
      alternative_service_info =
          AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
              alternative_service, expiration, advertised_versions);
    } else {
      alternative_service_info =
          AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
              alternative_service, expiration);
    }
    alternative_service_info_vector.push_back(alternative_service_info);
  }
  return alternative_service_info_vector;
}

}  // namespace net
