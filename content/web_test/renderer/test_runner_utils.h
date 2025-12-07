// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_TEST_RUNNER_UTILS_H_
#define CONTENT_WEB_TEST_RENDERER_TEST_RUNNER_UTILS_H_

#include <map>
#include <string>

namespace content {

class TestRunnerUtils {
 public:
  // Parse the command line arguments from "key1:value1,key2:value2" format to a
  // map of {"key1": "value1", "key2": "value2"}.
  static std::map<std::string_view, std::string_view> ParseWebSettingsString(
      std::string_view web_settings);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_TEST_RUNNER_UTILS_H_
