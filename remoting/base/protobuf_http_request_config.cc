// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_request_config.h"

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting {

// static
scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy>
ProtobufHttpRequestConfig::GetSimpleRetryPolicy() {
  static constexpr net::BackoffEntry::Policy kBackoffPolicy = {
      .num_errors_to_ignore = 0,
      .initial_delay_ms = base::Seconds(1).InMilliseconds(),
      .multiply_factor = 2,
      .jitter_factor = 0.5,
      .maximum_backoff_ms = base::Seconds(30).InMilliseconds(),
      .entry_lifetime_ms = -1,  // never discard.
      // InformOfRequest() is called before the retry task is scheduled, so the
      // initial delay is technically used.
      .always_use_initial_delay = false,
  };

  static base::NoDestructor<scoped_refptr<RetryPolicy>> policy([]() {
    auto policy = base::MakeRefCounted<RetryPolicy>();
    policy->backoff_policy = &kBackoffPolicy;
    policy->retry_timeout = base::Minutes(1);
    return policy;
  }());

  return *policy;
}

ProtobufHttpRequestConfig::ProtobufHttpRequestConfig(
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : traffic_annotation(traffic_annotation) {}

ProtobufHttpRequestConfig::~ProtobufHttpRequestConfig() = default;

void ProtobufHttpRequestConfig::Validate() const {
  DCHECK(request_message);
  DCHECK(!path.empty());
}

void ProtobufHttpRequestConfig::UseSimpleRetryPolicy() {
  retry_policy = GetSimpleRetryPolicy();
}

ProtobufHttpRequestConfig::RetryPolicy::RetryPolicy() = default;

}  // namespace remoting
