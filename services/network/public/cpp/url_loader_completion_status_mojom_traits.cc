// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_loader_completion_status_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "services/network/public/cpp/network_param_mojom_traits.h"

namespace mojo {

bool StructTraits<network::mojom::BlockedByResponseReasonWrapperDataView,
                  network::mojom::BlockedByResponseReason>::
    Read(network::mojom::BlockedByResponseReasonWrapperDataView data,
         network::mojom::BlockedByResponseReason* out) {
  *out = data.reason();
  return true;
}

bool StructTraits<network::mojom::URLLoaderCompletionStatusDataView,
                  network::URLLoaderCompletionStatus>::
    Read(network::mojom::URLLoaderCompletionStatusDataView data,
         network::URLLoaderCompletionStatus* out) {
  if (!data.ReadCompletionTime(&out->completion_time) ||
      !data.ReadCorsErrorStatus(&out->cors_error_status) ||
      !data.ReadPrivateNetworkAccessPreflightResult(
          &out->private_network_access_preflight_result) ||
      !data.ReadTrustTokenOperationStatus(&out->trust_token_operation_status) ||
      !data.ReadSslInfo(&out->ssl_info) ||
      !data.ReadBlockedByResponseReason(&out->blocked_by_response_reason) ||
      !data.ReadResolveErrorInfo(&out->resolve_error_info)) {
    return false;
  }

  out->error_code = data.error_code();
  out->extended_error_code = data.extended_error_code();
  out->exists_in_cache = data.exists_in_cache();
  out->exists_in_memory_cache = data.exists_in_memory_cache();
  out->encoded_data_length = data.encoded_data_length();
  out->encoded_body_length = data.encoded_body_length();
  out->decoded_body_length = data.decoded_body_length();
  out->should_report_orb_blocking = data.should_report_orb_blocking();
  out->should_collapse_initiator = data.should_collapse_initiator();
  return true;
}

}  // namespace mojo
