// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_COMPLETION_STATUS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_COMPLETION_STATUS_H_

#include <stdint.h>

#include <optional>

#include "base/component_export.h"
#include "base/time/time.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom-shared.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace network {

// NOTE: When adding/removing fields to this struct, don't forget to update
// services/network/public/cpp/network_ipc_param_traits.h and the equals (==)
// operator below.

struct COMPONENT_EXPORT(NETWORK_CPP_BASE) URLLoaderCompletionStatus {
  URLLoaderCompletionStatus();
  URLLoaderCompletionStatus(const URLLoaderCompletionStatus& status);

  // Sets |error_code| to |error_code| and base::TimeTicks::Now() to
  // |completion_time|.
  explicit URLLoaderCompletionStatus(int error_code);

  // Sets ERR_FAILED to |error_code|, |error| to |cors_error_status|, and
  // base::TimeTicks::Now() to |completion_time|.
  explicit URLLoaderCompletionStatus(const CorsErrorStatus& error);

  // Sets ERR_BLOCKED_BY_RESPONSE to |error_code|, |reason| to
  // |blocked_by_response_reason|, and base::TimeTicks::Now() to
  // |completion_time|.
  explicit URLLoaderCompletionStatus(
      const mojom::BlockedByResponseReason& reason);

  ~URLLoaderCompletionStatus();

  bool operator==(const URLLoaderCompletionStatus& rhs) const;

  // The error code. ERR_FAILED is set for CORS errors.
  int error_code = 0;

  // Extra detail on the error.
  int extended_error_code = 0;

  // A copy of the data requested exists in the disk cache and/or the in-memory
  // cache.
  bool exists_in_cache = false;

  // A copy of the data requested exists in the in-memory cache.
  bool exists_in_memory_cache = false;

  // Time the request completed.
  base::TimeTicks completion_time;

  // Total amount of data received from the network.
  int64_t encoded_data_length = 0;

  // The length of the response body before removing any content encodings.
  int64_t encoded_body_length = 0;

  // The length of the response body after decoding.
  int64_t decoded_body_length = 0;

  // Optional CORS error details.
  std::optional<CorsErrorStatus> cors_error_status;

  // Information about any preflight request sent for Private Network Access
  // as part of this load, that was not previously reported in
  // `URLResponseHead`.
  mojom::PrivateNetworkAccessPreflightResult
      private_network_access_preflight_result =
          mojom::PrivateNetworkAccessPreflightResult::kNone;

  // Optional Trust Tokens (https://github.com/wicg/trust-token-api) error
  // details.
  //
  // A non-kOk value denotes that the request failed because a Trust Tokens
  // operation was attempted and failed for the given reason.
  //
  // The status is set to kOk in all other cases. In particular, a value of kOk
  // does not imply that a Trust Tokens operation was executed successfully
  // alongside this request, or even that a Trust Tokens operation was
  // attempted.
  mojom::TrustTokenOperationStatus trust_token_operation_status =
      mojom::TrustTokenOperationStatus::kOk;

  // Optional SSL certificate info.
  std::optional<net::SSLInfo> ssl_info;

  // More detailed reason for failing the response with
  // net::ERR_BLOCKED_BY_RESPONSE |error_code|.
  std::optional<mojom::BlockedByResponseReason> blocked_by_response_reason;

  // Set when response blocked by ORB needs to be reported to the DevTools
  // console.
  bool should_report_orb_blocking = false;

  // Host resolution error info for this request.
  net::ResolveErrorInfo resolve_error_info;

  // Whether the initiator of this request should be collapsed.
  bool should_collapse_initiator = false;

  // Write a representation of this struct into a trace.
  void WriteIntoTrace(perfetto::TracedValue context) const;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_COMPLETION_STATUS_H_
