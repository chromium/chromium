// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/properties_based_quic_server_info.h"

#include "base/base64.h"
#include "base/metrics/histogram_macros.h"
#include "net/base/net_errors.h"
#include "net/http/http_server_properties.h"

using std::string;

namespace {

void RecordQuicServerInfoFailure(net::QuicServerInfo::FailureReason failure) {
  UMA_HISTOGRAM_ENUMERATION(
      "Net.QuicDiskCache.FailureReason.PropertiesBasedCache", failure,
      net::QuicServerInfo::NUM_OF_FAILURES);
}

}  // namespace

namespace net {

PropertiesBasedQuicServerInfo::PropertiesBasedQuicServerInfo(
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key,
    HttpServerProperties* http_server_properties)
    : QuicServerInfo(server_id),
      privacy_mode_(privacy_mode),
      network_anonymization_key_(network_anonymization_key),
      http_server_properties_(http_server_properties) {
  DCHECK(http_server_properties_);
}

PropertiesBasedQuicServerInfo::~PropertiesBasedQuicServerInfo() = default;

bool PropertiesBasedQuicServerInfo::Load() {
  const string* data = http_server_properties_->GetQuicServerInfo(
      server_id_, privacy_mode_, network_anonymization_key_);
  string decoded;
  if (!data) {
    RecordQuicServerInfoFailure(PARSE_NO_DATA_FAILURE);
    return false;
  }
  if (!base::Base64Decode(*data, &decoded)) {
    RecordQuicServerInfoFailure(PARSE_DATA_DECODE_FAILURE);
    return false;
  }
  if (!Parse(decoded)) {
    RecordQuicServerInfoFailure(PARSE_FAILURE);
    return false;
  }
  return true;
}

void PropertiesBasedQuicServerInfo::Persist() {
  string encoded = base::Base64Encode(Serialize());
  http_server_properties_->SetQuicServerInfo(
      server_id_, privacy_mode_, network_anonymization_key_, encoded);
}

}  // namespace net
