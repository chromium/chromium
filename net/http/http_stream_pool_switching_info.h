// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_SWITCHING_INFO_H_
#define NET_HTTP_HTTP_STREAM_POOL_SWITCHING_INFO_H_

#include "net/base/net_export.h"
#include "net/http/alternative_service.h"
#include "net/http/http_stream_key.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

// Contains information for switching to the HttpStreamPool.
struct NET_EXPORT_PRIVATE HttpStreamPoolSwitchingInfo {
  HttpStreamPoolSwitchingInfo(HttpStreamKey stream_key,
                              AlternativeServiceInfo alternative_service_info,
                              quic::ParsedQuicVersion quic_version,
                              bool is_http1_allowed,
                              int load_flags,
                              ProxyInfo proxy_info);

  HttpStreamPoolSwitchingInfo(HttpStreamPoolSwitchingInfo&&);
  HttpStreamPoolSwitchingInfo& operator=(HttpStreamPoolSwitchingInfo&&);

  HttpStreamPoolSwitchingInfo(const HttpStreamPoolSwitchingInfo&) = delete;
  HttpStreamPoolSwitchingInfo& operator=(const HttpStreamPoolSwitchingInfo&) =
      delete;

  ~HttpStreamPoolSwitchingInfo();

  HttpStreamKey stream_key;
  AlternativeServiceInfo alternative_service_info;
  quic::ParsedQuicVersion quic_version;
  bool is_http1_allowed;
  int load_flags = 0;
  ProxyInfo proxy_info;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_SWITCHING_INFO_H_
