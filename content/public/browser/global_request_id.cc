// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/global_request_id.h"

#include <atomic>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace content {

void GlobalRequestID::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("child_id", child_id);
  dict.Add("request_id", request_id);
}

// This method must not be inlined, or it might reuse `request_id` values.
//
// static
NOINLINE GlobalRequestID GlobalRequestID::MakeBrowserInitiated() {
  static constinit std::atomic<int32_t> s_next_request_id{-2};
  // Note that atomic decrement for signed integral types is defined in terms of
  // two's complement. Decrementing the min `int32_t` (0x80000000) will wrap
  // around to the max (0x7fffffff). There are no undefined results.
  const int32_t request_id = s_next_request_id--;
  CHECK_LT(request_id, 0);
  return GlobalRequestID(-1, request_id);
}

}  // namespace content
