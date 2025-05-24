// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/alternative_service.h"

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "net/base/port_util.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/spdy_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

void HistogramAlternateProtocolUsage(AlternateProtocolUsage usage,
                                     bool is_google_host) {
  UMA_HISTOGRAM_ENUMERATION("Net.AlternateProtocolUsage", usage,
                            ALTERNATE_PROTOCOL_USAGE_MAX);
  if (is_google_host) {
    UMA_HISTOGRAM_ENUMERATION("Net.AlternateProtocolUsageGoogle", usage,
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
    case NextProto::kProtoUnknown:
      return false;
    case NextProto::kProtoHTTP11:
      return false;
    case NextProto::kProtoHTTP2:
      return true;
    case NextProto::kProtoQUIC:
      return true;
  }
  NOTREACHED();
}

bool IsProtocolEnabled(NextProto protocol,
                       bool is_http2_enabled,
                       bool is_quic_enabled) {
  switch (protocol) {
    case NextProto::kProtoUnknown:
      NOTREACHED();
    case NextProto::kProtoHTTP11:
      return true;
    case NextProto::kProtoHTTP2:
      return is_http2_enabled;
    case NextProto::kProtoQUIC:
      return is_quic_enabled;
  }
  NOTREACHED();
}

AlternativeService::AlternativeService(NextProto protocol,
                                       std::string_view host,
                                       uint16_t port)
    : protocol(protocol), host(host), port(port) {}

AlternativeService::AlternativeService(NextProto protocol,
                                       const HostPortPair& host_port_pair)
    : AlternativeService(protocol,
                         host_port_pair.host(),
                         host_port_pair.port()) {}

AlternativeService::AlternativeService(
    const AlternativeService& alternative_service) = default;
AlternativeService::AlternativeService(AlternativeService&&) noexcept = default;

AlternativeService& AlternativeService::operator=(
    const AlternativeService& alternative_service) = default;
AlternativeService& AlternativeService::operator=(AlternativeService&&) =
    default;

HostPortPair AlternativeService::GetHostPortPair() const {
  return HostPortPair(host, port);
}

std::strong_ordering AlternativeService::operator<=>(
    const AlternativeService& other) const = default;

std::string AlternativeService::ToString() const {
  return base::StringPrintf("%s %s:%d", NextProtoToString(protocol),
                            host.c_str(), port);
}

std::ostream& operator<<(std::ostream& os,
                         const AlternativeService& alternative_service) {
  os << alternative_service.ToString();
  return os;
}

// static
AlternativeServiceInfo
AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
    const AlternativeService& alternative_service,
    base::Time expiration) {
  DCHECK_EQ(alternative_service.protocol, NextProto::kProtoHTTP2);
  return AlternativeServiceInfo(alternative_service, expiration,
                                quic::ParsedQuicVersionVector());
}

// static
AlternativeServiceInfo AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
    const AlternativeService& alternative_service,
    base::Time expiration,
    const quic::ParsedQuicVersionVector& advertised_versions) {
  DCHECK_EQ(alternative_service.protocol, NextProto::kProtoQUIC);
  return AlternativeServiceInfo(alternative_service, expiration,
                                advertised_versions);
}

AlternativeServiceInfo::AlternativeServiceInfo() = default;

AlternativeServiceInfo::AlternativeServiceInfo(
    const AlternativeServiceInfo& alternative_service_info) = default;
AlternativeServiceInfo::AlternativeServiceInfo(
    AlternativeServiceInfo&&) noexcept = default;

AlternativeServiceInfo& AlternativeServiceInfo::operator=(
    AlternativeServiceInfo&&) = default;
AlternativeServiceInfo& AlternativeServiceInfo::operator=(
    const AlternativeServiceInfo& alternative_service_info) = default;

AlternativeServiceInfo::~AlternativeServiceInfo() = default;

bool AlternativeServiceInfo::operator==(
    const AlternativeServiceInfo& other) const = default;

std::string AlternativeServiceInfo::ToString() const {
  // NOTE: Cannot use `base::UnlocalizedTimeFormatWithPattern()` since
  // `net/DEPS` disallows `base/i18n`.
  base::Time::Exploded exploded;
  expiration_.LocalExplode(&exploded);
  return base::StringPrintf(
      "%s, expires %04d-%02d-%02d %02d:%02d:%02d",
      alternative_service_.ToString().c_str(), exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second);
}

void AlternativeServiceInfo::SetAdvertisedVersions(
    const quic::ParsedQuicVersionVector& advertised_versions) {
  if (alternative_service_.protocol != NextProto::kProtoQUIC) {
    return;
  }

  advertised_versions_ = advertised_versions;
  std::ranges::sort(advertised_versions_, {},
                    &quic::ParsedQuicVersion::transport_version);
}

AlternativeServiceInfoVector ProcessAlternativeServices(
    const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector&
        alternative_service_vector,
    bool is_http2_enabled,
    bool is_quic_enabled,
    const quic::ParsedQuicVersionVector& supported_quic_versions) {
  // Convert spdy::SpdyAltSvcWireFormat::AlternativeService entries
  // to AlternativeServiceInfo.
  AlternativeServiceInfoVector alternative_service_info_vector;
  for (const spdy::SpdyAltSvcWireFormat::AlternativeService&
           alternative_service_entry : alternative_service_vector) {
    if (!IsPortValid(alternative_service_entry.port)) {
      continue;
    }

    NextProto protocol =
        NextProtoFromString(alternative_service_entry.protocol_id);
    quic::ParsedQuicVersionVector advertised_versions;
    if (protocol == NextProto::kProtoQUIC) {
      continue;  // Ignore legacy QUIC alt-svc advertisements.
    } else if (!IsAlternateProtocolValid(protocol)) {
      quic::ParsedQuicVersion version =
          quic::SpdyUtils::ExtractQuicVersionFromAltSvcEntry(
              alternative_service_entry, supported_quic_versions);
      if (version == quic::ParsedQuicVersion::Unsupported()) {
        continue;
      }
      protocol = NextProto::kProtoQUIC;
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
        base::Seconds(alternative_service_entry.max_age_seconds);
    AlternativeServiceInfo alternative_service_info;
    if (protocol == NextProto::kProtoQUIC) {
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

AlternativeServiceInfo::AlternativeServiceInfo(
    const AlternativeService& alternative_service,
    base::Time expiration,
    const quic::ParsedQuicVersionVector& advertised_versions)
    : alternative_service_(alternative_service), expiration_(expiration) {
  if (alternative_service_.protocol == NextProto::kProtoQUIC) {
    advertised_versions_ = advertised_versions;
  }
}

}  // namespace net
