// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_NAVIGATION_CAPTURING_LOG_H_
#define COMPONENTS_WEBAPPS_BROWSER_NAVIGATION_CAPTURING_LOG_H_

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string_view>

namespace base {
class Value;
}

namespace web_app {

// Stores debug information surfaced in chrome://web-app-internals and printed
// in failing tests.
class NavigationCapturingLog {
 public:
  explicit NavigationCapturingLog(size_t max_log_entries);
  ~NavigationCapturingLog();

  void LogData(std::string_view source,
               base::Value value,
               std::optional<int64_t> navigation_handle_id);

  // This cannot be used for any production logic.
  base::Value GetLog() const;

 private:
  const size_t max_log_entries_;
  std::list<base::Value> debug_log_;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_BROWSER_NAVIGATION_CAPTURING_LOG_H_
