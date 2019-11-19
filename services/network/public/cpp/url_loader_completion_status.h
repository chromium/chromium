// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_COMPLETION_STATUS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_COMPLETION_STATUS_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/proxy_server.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/cors.mojom-shared.h"

namespace network {

// NOTE: When adding/removing fields to this struct, don't forget to
// update services/network/public/cpp/network_ipc_param_traits.h.

struct COMPONENT_EXPORT(NETWORK_CPP_BASE) URLLoaderCompletionStatus {
  URLLoaderCompletionStatus();
  URLLoaderCompletionStatus(const URLLoaderCompletionStatus& status);

  // Sets |error_code| to |error_code| and base::TimeTicks::Now() to
  // |completion_time|.
  explicit URLLoaderCompletionStatus(int error_code);

  // Sets ERR_FAILED to |error_code|, |error| to |cors_error_status|, and
  // base::TimeTicks::Now() to |completion_time|.
  explicit URLLoaderCompletionStatus(const CorsErrorStatus& error);

  ~URLLoaderCompletionStatus();

  bool operator==(const URLLoaderCompletionStatus& rhs) const;

  // The error code. ERR_FAILED is set for CORS errors.
  int error_code = 0;

  // Extra detail on the error.
  int extended_error_code = 0;

  // A copy of the data requested exists in the cache.
  bool exists_in_cache = false;

  // Time the request completed.
  base::TimeTicks completion_time;

  // Total amount of data received from the network.
  int64_t encoded_data_length = 0;

  // The length of the response body before removing any content encodings.
  int64_t encoded_body_length = 0;

  // The length of the response body after decoding.
  int64_t decoded_body_length = 0;

  // Optional CORS error details.
  base::Optional<CorsErrorStatus> cors_error_status;

  // Optional SSL certificate info.
  base::Optional<net::SSLInfo> ssl_info;

  // Set when response blocked by CORB needs to be reported to the DevTools
  // console.
  bool should_report_corb_blocking = false;

  // The proxy server used for this request, if any.
  net::ProxyServer proxy_server;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_COMPLETION_STATUS_H_
