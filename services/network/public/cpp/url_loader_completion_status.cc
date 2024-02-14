// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_loader_completion_status.h"

#include "base/trace_event/trace_event.h"
#include "net/base/net_errors.h"

namespace network {

URLLoaderCompletionStatus::URLLoaderCompletionStatus() = default;
URLLoaderCompletionStatus::URLLoaderCompletionStatus(
    const URLLoaderCompletionStatus& status) = default;

URLLoaderCompletionStatus::URLLoaderCompletionStatus(int error_code)
    : error_code(error_code), completion_time(base::TimeTicks::Now()) {}

URLLoaderCompletionStatus::URLLoaderCompletionStatus(
    const CorsErrorStatus& error)
    : URLLoaderCompletionStatus(net::ERR_FAILED) {
  cors_error_status = error;
}

URLLoaderCompletionStatus::URLLoaderCompletionStatus(
    const mojom::BlockedByResponseReason& reason)
    : URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_RESPONSE) {
  blocked_by_response_reason = reason;
}

URLLoaderCompletionStatus::~URLLoaderCompletionStatus() = default;

bool URLLoaderCompletionStatus::operator==(
    const URLLoaderCompletionStatus& rhs) const {
  return error_code == rhs.error_code &&
         extended_error_code == rhs.extended_error_code &&
         exists_in_cache == rhs.exists_in_cache &&
         exists_in_memory_cache == rhs.exists_in_memory_cache &&
         completion_time == rhs.completion_time &&
         encoded_data_length == rhs.encoded_data_length &&
         encoded_body_length == rhs.encoded_body_length &&
         decoded_body_length == rhs.decoded_body_length &&
         cors_error_status == rhs.cors_error_status &&
         private_network_access_preflight_result ==
             rhs.private_network_access_preflight_result &&
         blocked_by_response_reason == rhs.blocked_by_response_reason &&
         should_report_orb_blocking == rhs.should_report_orb_blocking &&
         should_collapse_initiator == rhs.should_collapse_initiator;
}

void URLLoaderCompletionStatus::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("error_code", error_code);
  dict.Add("extended_error_code", extended_error_code);
  dict.Add("encoded_data_length", encoded_data_length);
  dict.Add("encoded_body_length", encoded_body_length);
  dict.Add("decoded_body_length", decoded_body_length);
}

}  // namespace network
