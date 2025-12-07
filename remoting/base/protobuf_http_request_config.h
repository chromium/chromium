// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_REQUEST_CONFIG_H_
#define REMOTING_BASE_PROTOBUF_HTTP_REQUEST_CONFIG_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
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
  struct RetryPolicy : public base::RefCountedThreadSafe<RetryPolicy> {
    RetryPolicy();
    // `backoff_policy` must outlive `this`. In most cases you want to define
    // the policy as a `static constexpr` then set this to point to it.
    raw_ptr<const net::BackoffEntry::Policy> backoff_policy;

    // A duration counted from when the first attempt of the request is made,
    // after which the request will no longer be retried.
    base::TimeDelta retry_timeout;

   private:
    friend class base::RefCountedThreadSafe<RetryPolicy>;

    ~RetryPolicy() = default;
  };

  // Returns a simple RetryPolicy that retries for up to ~1 minute (counted
  // from when the first attempt of the request is made) with exponential
  // backoff, suitable for most simple short running operations.
  static scoped_refptr<const RetryPolicy> GetSimpleRetryPolicy();

  explicit ProtobufHttpRequestConfig(
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~ProtobufHttpRequestConfig();

  // Runs DCHECK's on the fields to make sure all fields have been set up.
  void Validate() const;

  // This is equivalent to setting `retry_policy` to GetSimpleRetryPolicy().
  void UseSimpleRetryPolicy();

  const net::NetworkTrafficAnnotationTag traffic_annotation;
  std::unique_ptr<google::protobuf::MessageLite> request_message;
  std::string path;
  bool authenticated = true;
  bool provide_certificate = false;
  std::string method = net::HttpRequestHeaders::kPostMethod;

  // Optional. Only needed when the request requires an API key.
  std::string api_key;

  // If configured, request will be automatically retried if the error code is
  // one of ABORTED, UNAVAILABLE, or NETWORK_ERROR. Set this to null to disable
  // retries.
  // NOTE: `retry_policy` is currently not supported by stream requests.
  scoped_refptr<const RetryPolicy> retry_policy;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_REQUEST_CONFIG_H_
