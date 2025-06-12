// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_REQUEST_CONFIG_H_
#define REMOTING_BASE_PROTOBUF_HTTP_REQUEST_CONFIG_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/http_status.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace remoting {

// Common configurations for unary and stream protobuf http requests. Caller
// needs to set all fields in this struct unless otherwise documented.
struct ProtobufHttpRequestConfig {
  struct RetryPolicy {
    // `backoff_policy` must outlive `this`. In most cases you want to define
    // the policy as a `static constexpr` then set this to point to it.
    raw_ptr<const net::BackoffEntry::Policy> backoff_policy;

    // A deadline after which the request will no longer be retried.
    base::TimeTicks retry_deadline;
  };

  // Helper function to create a unique_ptr of RetryPolicy. See comments in the
  // struct.
  static std::unique_ptr<RetryPolicy> CreateRetryPolicy(
      const net::BackoffEntry::Policy& backoff_policy,
      base::TimeTicks retry_deadline);

  // Creates the default RetryPolicy that retries for up to ~1 minute (counted
  // from when this method is called) with exponential backoff, suitable for
  // most simple short running operations.
  static std::unique_ptr<RetryPolicy> CreateDefaultRetryPolicy();

  explicit ProtobufHttpRequestConfig(
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~ProtobufHttpRequestConfig();

  // Runs DCHECK's on the fields to make sure all fields have been set up.
  void Validate() const;

  const net::NetworkTrafficAnnotationTag traffic_annotation;
  std::unique_ptr<google::protobuf::MessageLite> request_message;
  std::string path;
  bool authenticated = true;
  bool provide_certificate = false;
  std::string method = net::HttpRequestHeaders::kPostMethod;

  // Optional. Only needed when the request requires an API key.
  std::string api_key;

  // If configured, request will be automatically retried if the error code
  // is one of ABORTED, UNAVAILABLE, or NETWORK_ERROR.
  // NOTE: `retry_policy` is currently not supported by stream requests.
  std::unique_ptr<RetryPolicy> retry_policy;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_REQUEST_CONFIG_H_
