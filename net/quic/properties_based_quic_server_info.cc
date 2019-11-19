// Copyright 2015 The Chromium Authors. All rights reserved.
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
    const NetworkIsolationKey& network_isolation_key,
    HttpServerProperties* http_server_properties)
    : QuicServerInfo(server_id),
      network_isolation_key_(network_isolation_key),
      http_server_properties_(http_server_properties) {
  DCHECK(http_server_properties_);
}

PropertiesBasedQuicServerInfo::~PropertiesBasedQuicServerInfo() {}

bool PropertiesBasedQuicServerInfo::Load() {
  const string* data = http_server_properties_->GetQuicServerInfo(
      server_id_, network_isolation_key_);
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
  string encoded;
  base::Base64Encode(Serialize(), &encoded);
  http_server_properties_->SetQuicServerInfo(server_id_, network_isolation_key_,
                                             encoded);
}

size_t PropertiesBasedQuicServerInfo::EstimateMemoryUsage() const {
  return 0;
}

}  // namespace net
