// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_COMPLETION_STATUS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_COMPLETION_STATUS_MOJOM_TRAITS_H_

#include <optional>

#include "base/component_export.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/cors/cors_mojom_traits.h"
#include "services/network/public/cpp/net_ipc_param_traits.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom-shared.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom-shared.h"

namespace mojo {

// The wrapper struct is effectively an alias of the wrapped enum, so map them.
template <>
class COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::BlockedByResponseReasonWrapperDataView,
                 network::mojom::BlockedByResponseReason> {
 public:
  static network::mojom::BlockedByResponseReason reason(
      network::mojom::BlockedByResponseReason reason) {
    return reason;
  }

  static bool Read(network::mojom::BlockedByResponseReasonWrapperDataView data,
                   network::mojom::BlockedByResponseReason* out);
};

template <>
class COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::URLLoaderCompletionStatusDataView,
                 network::URLLoaderCompletionStatus> {
 public:
  static int32_t error_code(const network::URLLoaderCompletionStatus& status) {
    return status.error_code;
  }

  static int32_t extended_error_code(
      const network::URLLoaderCompletionStatus& status) {
    return status.extended_error_code;
  }

  static bool exists_in_cache(
      const network::URLLoaderCompletionStatus& status) {
    return status.exists_in_cache;
  }

  static bool exists_in_memory_cache(
      const network::URLLoaderCompletionStatus& status) {
    return status.exists_in_memory_cache;
  }

  static const base::TimeTicks& completion_time(
      const network::URLLoaderCompletionStatus& status) {
    return status.completion_time;
  }

  static int64_t encoded_data_length(
      const network::URLLoaderCompletionStatus& status) {
    return status.encoded_data_length;
  }

  static int64_t encoded_body_length(
      const network::URLLoaderCompletionStatus& status) {
    return status.encoded_body_length;
  }

  static int64_t decoded_body_length(
      const network::URLLoaderCompletionStatus& status) {
    return status.decoded_body_length;
  }

  static const std::optional<network::CorsErrorStatus>& cors_error_status(
      const network::URLLoaderCompletionStatus& status) {
    return status.cors_error_status;
  }

  static network::mojom::PrivateNetworkAccessPreflightResult
  private_network_access_preflight_result(
      const network::URLLoaderCompletionStatus& status) {
    return status.private_network_access_preflight_result;
  }

  static network::mojom::TrustTokenOperationStatus trust_token_operation_status(
      const network::URLLoaderCompletionStatus& status) {
    return status.trust_token_operation_status;
  }

  static const std::optional<net::SSLInfo>& ssl_info(
      const network::URLLoaderCompletionStatus& status) {
    return status.ssl_info;
  }

  static const std::optional<network::mojom::BlockedByResponseReason>&
  blocked_by_response_reason(const network::URLLoaderCompletionStatus& status) {
    return status.blocked_by_response_reason;
  }

  static bool should_report_orb_blocking(
      const network::URLLoaderCompletionStatus& status) {
    return status.should_report_orb_blocking;
  }

  static const net::ResolveErrorInfo& resolve_error_info(
      const network::URLLoaderCompletionStatus& status) {
    return status.resolve_error_info;
  }

  static bool should_collapse_initiator(
      const network::URLLoaderCompletionStatus& status) {
    return status.should_collapse_initiator;
  }

  static bool Read(network::mojom::URLLoaderCompletionStatusDataView data,
                   network::URLLoaderCompletionStatus* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_COMPLETION_STATUS_MOJOM_TRAITS_H_
