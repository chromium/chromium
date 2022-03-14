// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SEND_RESULT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SEND_RESULT_H_

#include "content/common/content_export.h"

namespace content {

// Struct that contains data about sent reports. Some info is displayed in the
// Conversion Internals WebUI.
struct CONTENT_EXPORT SendResult {
  enum class Status {
    kSent,
    // The report failed without receiving response headers.
    kTransientFailure,
    // The report failed due to other cause and would not be retried.
    kFailure,
    // The report was dropped without ever being sent, e.g. due to embedder
    // disabling the API.
    kDropped,
    // The report was dropped without ever being sent because assembly failed,
    // e.g. the aggregation service was unavailable.
    kFailedToAssemble,
  };

  SendResult(Status status, int http_response_code)
      : status(status), http_response_code(http_response_code) {}
  SendResult(const SendResult& other) = default;
  SendResult& operator=(const SendResult& other) = default;
  SendResult(SendResult&& other) = default;
  SendResult& operator=(SendResult&& other) = default;
  ~SendResult() = default;

  Status status;

  // Information on the network request that was sent.
  int http_response_code;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SEND_RESULT_H_
