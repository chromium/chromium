// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/test_runner_utils.h"

#include <string_view>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace content {

// static
std::map<std::string_view, std::string_view>
TestRunnerUtils::ParseWebSettingsString(std::string_view web_settings) {
  std::map<std::string_view, std::string_view> settings;
  base::StringViewPairs key_value_pairs;
  base::SplitStringIntoKeyValueViewPairs(web_settings, ':', ',',
                                         &key_value_pairs);
  for (auto& [key, value] : key_value_pairs) {
    std::string_view trimmed_key =
        base::TrimWhitespaceASCII(key, base::TRIM_ALL);
    std::string_view trimmed_value =
        base::TrimWhitespaceASCII(value, base::TRIM_ALL);
    if (!trimmed_key.empty() && !trimmed_value.empty()) {
      settings[trimmed_key] = trimmed_value;
    }
  }

  return settings;
}

}  // namespace content
