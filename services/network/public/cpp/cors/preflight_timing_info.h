// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_PREFLIGHT_TIMING_INFO_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_PREFLIGHT_TIMING_INFO_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "services/network/public/mojom/cors.mojom-shared.h"

namespace network {

namespace cors {

// Stores performance monitoring information for CORS preflight requests that
// are made in the NetworkService. Will be used to carry information from the
// NetworkService to call sites via URLLoaderCompletionStatus.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) PreflightTimingInfo {
  PreflightTimingInfo();
  PreflightTimingInfo(const PreflightTimingInfo& info);
  ~PreflightTimingInfo();

  base::TimeTicks start_time;
  base::TimeTicks finish_time;
  std::string alpn_negotiated_protocol;
  net::HttpResponseInfo::ConnectionInfo connection_info =
      net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN;
  std::string timing_allow_origin;
  uint64_t transfer_size = 0;

  bool operator==(const PreflightTimingInfo& rhs) const;
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_PREFLIGHT_TIMING_INFO_H_
