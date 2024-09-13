// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_switching_info.h"

#include "net/http/alternative_service.h"
#include "net/http/http_stream_key.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

HttpStreamPoolSwitchingInfo::HttpStreamPoolSwitchingInfo(
    HttpStreamKey stream_key,
    AlternativeServiceInfo alternative_service_info,
    quic::ParsedQuicVersion quic_version,
    bool is_http1_allowed,
    int load_flags,
    ProxyInfo proxy_info)
    : stream_key(std::move(stream_key)),
      alternative_service_info(std::move(alternative_service_info)),
      quic_version(quic_version),
      is_http1_allowed(is_http1_allowed),
      load_flags(load_flags),
      proxy_info(std::move(proxy_info)) {}

HttpStreamPoolSwitchingInfo::HttpStreamPoolSwitchingInfo(
    HttpStreamPoolSwitchingInfo&&) = default;

HttpStreamPoolSwitchingInfo& HttpStreamPoolSwitchingInfo::operator=(
    HttpStreamPoolSwitchingInfo&&) = default;

HttpStreamPoolSwitchingInfo::~HttpStreamPoolSwitchingInfo() = default;

}  // namespace net
