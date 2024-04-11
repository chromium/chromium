// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_BLINK_TEST_HELPERS_H_
#define CONTENT_WEB_TEST_RENDERER_BLINK_TEST_HELPERS_H_

#include <string_view>

#include "third_party/blink/public/platform/web_url.h"

namespace blink {
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace content {
struct TestPreferences;

// The TestRunner library keeps its settings in a TestPreferences object.
// The content_shell, however, uses WebPreferences. This method exports the
// settings from the TestRunner library which are relevant for web tests.
void ExportWebTestSpecificPreferences(const TestPreferences& from,
                                      blink::web_pref::WebPreferences* to);

// Rewrites a URL requested from a web test. There are two rules:
// 1. If the URL is an absolute file path requested from a WPT test like
//    'file:///resources/testharness.js', then return a file URL to the file
//    under WPT test directory. This is used only when the test is run manually
//    with content_shell without a web server.
// 2. If the URL starts with file:///tmp/web_tests/, then return a file URL
//    to a temporary file under the web_tests directory.
// 3. If the URL starts with file:///gen/, then return a file URL to the file
//    under the gen/ directory of the build out.
blink::WebURL RewriteWebTestsURL(std::string_view utf8_url, bool is_wpt_mode);

// Applies the rewrite rules except 1 of RewriteWebTestsURL().
blink::WebURL RewriteFileURLToLocalResource(std::string_view resource);

// Returns true if |test_url| points to a web platform test (WPT).
bool IsWebPlatformTest(std::string_view test_url);

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_BLINK_TEST_HELPERS_H_
