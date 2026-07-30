// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/navigation_capturing_log.h"

#include <optional>
#include <string_view>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"

namespace web_app {

NavigationCapturingLog::NavigationCapturingLog(size_t max_log_entries)
    : max_log_entries_(max_log_entries) {}
NavigationCapturingLog::~NavigationCapturingLog() = default;

void NavigationCapturingLog::LogData(
    std::string_view source,
    base::Value value,
    std::optional<int64_t> navigation_handle_id) {
  base::DictValue log_entry;
  log_entry.Set("source", source);
  log_entry.Set("navigation_id",
                base::saturated_cast<int>(navigation_handle_id.value_or(-1)));
  log_entry.Set("value", std::move(value));

  DVLOG(1) << log_entry.DebugString();
  debug_log_.emplace_front(std::move(log_entry));
  if (debug_log_.size() > max_log_entries_) {
    debug_log_.resize(max_log_entries_);
  }
}

base::Value NavigationCapturingLog::GetLog() const {
  base::ListValue log;
  for (const auto& command_value : debug_log_) {
    log.Append(command_value.Clone());
  }
  return base::Value(std::move(log));
}

}  // namespace web_app
