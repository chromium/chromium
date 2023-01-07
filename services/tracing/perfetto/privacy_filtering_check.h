// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_PRIVACY_FILTERING_CHECK_H_
#define SERVICES_TRACING_PERFETTO_PRIVACY_FILTERING_CHECK_H_

#include <string>

namespace tracing {

class PrivacyFilteringCheck {
 public:
  struct TraceStats {
    size_t track_event = 0;
    size_t process_desc = 0;
    size_t thread_desc = 0;

    bool has_interned_names = false;
    bool has_interned_categories = false;
    bool has_interned_source_locations = false;
    bool has_interned_log_messages = false;
  };

  PrivacyFilteringCheck();

  PrivacyFilteringCheck(const PrivacyFilteringCheck&) = delete;
  PrivacyFilteringCheck& operator=(const PrivacyFilteringCheck&) = delete;

  ~PrivacyFilteringCheck();

  // Removes disallowed fields from the trace.
  static void RemoveBlockedFields(std::string& serialized_trace_proto);

  void CheckProtoForUnexpectedFields(const std::string& serialized_trace_proto);

  const TraceStats& stats() const { return stats_; }

 private:
  TraceStats stats_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_PRIVACY_FILTERING_CHECK_H_
