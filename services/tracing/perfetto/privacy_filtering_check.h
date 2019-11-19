// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_PRIVACY_FILTERING_CHECK_H_
#define SERVICES_TRACING_PERFETTO_PRIVACY_FILTERING_CHECK_H_

#include <string>

#include "base/macros.h"

namespace tracing {

class PrivacyFilteringCheck {
 public:
  struct TraceStats {
    size_t track_event = 0;
    size_t process_desc = 0;
    size_t thread_desc = 0;

    size_t has_interned_names = 0;
    size_t has_interned_categories = 0;
    size_t has_interned_source_locations = 0;
  };

  PrivacyFilteringCheck();
  ~PrivacyFilteringCheck();

  void CheckProtoForUnexpectedFields(const std::string& serialized_trace_proto);

  const TraceStats& stats() const { return stats_; }

 private:
  TraceStats stats_;

  DISALLOW_COPY_AND_ASSIGN(PrivacyFilteringCheck);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_PRIVACY_FILTERING_CHECK_H_
