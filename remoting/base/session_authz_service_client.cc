// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/session_authz_service_client.h"

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "remoting/base/protobuf_http_request_config.h"

namespace remoting {

// static
scoped_refptr<ProtobufHttpRequestConfig::RetryPolicy>
SessionAuthzServiceClient::GetReauthRetryPolicy(
    base::TimeTicks token_expire_time) {
  static constexpr net::BackoffEntry::Policy kBackoffPolicy = {
      .num_errors_to_ignore = 0,
      .initial_delay_ms = base::Seconds(5).InMilliseconds(),
      .multiply_factor = 2,
      .jitter_factor = 0.5,
      .maximum_backoff_ms = base::Minutes(1).InMilliseconds(),
      .entry_lifetime_ms = -1,  // never discard.
      // InformOfRequest() is called before the retry task is scheduled, so the
      // initial delay is technically used.
      .always_use_initial_delay = false,
  };

  auto policy = base::MakeRefCounted<ProtobufHttpRequestConfig::RetryPolicy>();
  policy->backoff_policy = &kBackoffPolicy;
  // Add some leeway to account for network latencies.
  policy->retry_timeout =
      token_expire_time - base::TimeTicks::Now() - base::Seconds(5);
  return policy;
}

}  // namespace remoting
