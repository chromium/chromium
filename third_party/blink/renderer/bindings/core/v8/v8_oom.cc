// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_oom.h"

#include <ostream>

#include "base/logging.h"
#include "components/crash/core/common/crash_key.h"
#include "partition_alloc/oom.h"

namespace blink {

namespace {

struct PrintV8OOM {
  const char* location;
  const v8::OOMDetails& details;
};

std::ostream& operator<<(std::ostream& os, const PrintV8OOM& oom_details) {
  const auto [location, details] = oom_details;
  os << "V8 " << (details.is_heap_oom ? "javascript" : "process") << " OOM ("
     << location;
  if (details.detail) {
    os << "; detail: " << details.detail;
  }
  os << ").";
  return os;
}

}  // namespace

// Callback function called when V8 encounters an OOM error.
// Keep the implementation outside the anonymous namespace so that ChromeCrash
// recognizes it.
void ReportV8OOMError(const char* location, const v8::OOMDetails& details) {
  if (location) {
    static crash_reporter::CrashKeyString<64> location_key("v8-oom-location");
    location_key.Set(location);
  }

  if (details.detail) {
    static crash_reporter::CrashKeyString<128> detail_key("v8-oom-detail");
    detail_key.Set(details.detail);
  }

  LOG(ERROR) << PrintV8OOM{location, details};
  OOM_CRASH(0);
}

}  // namespace blink
