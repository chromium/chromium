// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "extensions/browser/api/content_settings/content_settings_helpers.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace helpers = content_settings_helpers;

TEST(ExtensionContentSettingsHelpersTest, ParseExtensionPattern) {
  static constexpr struct {
    const char* extension_pattern;
    const char* content_settings_pattern;
  } kTestPatterns[] = {
      {"<all_urls>", "*"},
      {"*://*.google.com/*", "[*.]google.com"},
      {"http://www.example.com/*", "http://www.example.com"},
      {"*://www.example.com/*", "www.example.com"},
      {"http://www.example.com:8080/*", "http://www.example.com:8080"},
      {"https://*/*", "https://*"},
      {"file:///foo/bar/baz", "file:///foo/bar/baz"},
  };
  for (const auto& entry : kTestPatterns) {
    std::string error;
    std::string pattern_str =
        helpers::ParseExtensionPattern(entry.extension_pattern, &error)
            .ToString();
    EXPECT_EQ(entry.content_settings_pattern, pattern_str)
        << "Unexpected error parsing " << entry.extension_pattern << ": "
        << error;
  }

  static constexpr struct {
    const char* extension_pattern;
    const char* expected_error;
  } kInvalidTestPatterns[] = {
      {"http://www.example.com/path", "Specific paths are not allowed."},
      {"file:///foo/bar/*",
       "Path wildcards in file URL patterns are not allowed."},
  };
  for (const auto& entry : kInvalidTestPatterns) {
    std::string error;
    ContentSettingsPattern pattern =
        helpers::ParseExtensionPattern(entry.extension_pattern, &error);
    EXPECT_FALSE(pattern.IsValid());
    EXPECT_EQ(entry.expected_error, error)
        << "Unexpected error parsing " << entry.extension_pattern;
  }
}

}  // namespace extensions
