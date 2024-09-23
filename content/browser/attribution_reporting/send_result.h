// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SEND_RESULT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SEND_RESULT_H_

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

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
    // The report was dropped without ever being sent because of unrecoverable
    // assembly failure, e.g. the aggregation service was unavailable.
    kAssemblyFailure,
    // The report was dropped because of transient assembly failure, e.g. the
    // public key was not fetched.
    kTransientAssemblyFailure,
  };

  struct Sent {
    enum class Result {
      kSent,
      kTransientFailure,
      kFailure,
    };

    Result result;
    int status;
    Sent(Result result, int status) : result(result), status(status) {}

    friend bool operator==(const Sent&, const Sent&) = default;
  };

  struct Dropped {};

  struct AssemblyFailure {
    bool transient;
    explicit AssemblyFailure(bool transient) : transient(transient) {}
  };

  Status status() const;

  using Result = absl::variant<Sent, Dropped, AssemblyFailure>;
  Result result;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SEND_RESULT_H_
