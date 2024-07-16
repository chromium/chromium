// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GLOBAL_REQUEST_ID_H_
#define CONTENT_PUBLIC_BROWSER_GLOBAL_REQUEST_ID_H_

#include <compare>

#include "base/numerics/safe_conversions.h"
#include "content/common/content_export.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace content {

// Uniquely identifies a net::URLRequest.
struct CONTENT_EXPORT GlobalRequestID {
  GlobalRequestID() = default;

  GlobalRequestID(base::StrictNumeric<int32_t> child_id,
                  base::StrictNumeric<int32_t> request_id)
      : child_id(child_id), request_id(request_id) {}

  // The unique ID of the child process (different from OS's PID).
  int32_t child_id = -1;

  // The request ID (unique for the child).
  int32_t request_id = -1;

  // Returns a Request ID for browser-initiated requests. Crashes if called more
  // than (2**31 - 2) times.
  static GlobalRequestID MakeBrowserInitiated();

  auto operator<=>(const GlobalRequestID& other) const = default;

  void WriteIntoTrace(perfetto::TracedValue context) const;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GLOBAL_REQUEST_ID_H_
