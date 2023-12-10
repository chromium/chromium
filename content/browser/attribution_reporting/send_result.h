// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SEND_RESULT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SEND_RESULT_H_

namespace content {

// Struct that contains data about sent reports. Some info is displayed in the
// Conversion Internals WebUI.
// TODO(apaseltiner): Consider replacing this struct with a single int that
// contains either HTTP response code, network error, or custom values for
// `Status::kDropped` and `Status::kFailedToAssemble`.
struct SendResult {
  enum class Status {
    kSent,
    // The report failed without receiving response headers.
    kTransientFailure,
    // The report failed due to other cause and would not be retried.
    kFailure,
    // The report was dropped without ever being sent, e.g. due to embedder
    // disabling the API.
    kDropped,
    // The report was dropped without ever being sent because of unrecoverable
    // assembly failure, e.g. the aggregation service was unavailable.
    kAssemblyFailure,
    // The report was dropped because of transient assembly failure, e.g. the
    // public key was not fetched.
    kTransientAssemblyFailure,
  };

  explicit SendResult(Status status,
                      int network_error = 0,
                      int http_response_code = 0)
      : status(status),
        network_error(network_error),
        http_response_code(http_response_code) {}

  SendResult(const SendResult&) = default;
  SendResult& operator=(const SendResult&) = default;

  SendResult(SendResult&&) = default;
  SendResult& operator=(SendResult&&) = default;

  ~SendResult() = default;

  Status status;

  // Information on the network request that was sent.
  int network_error;
  int http_response_code;

  friend bool operator==(const SendResult&, const SendResult&) = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SEND_RESULT_H_
