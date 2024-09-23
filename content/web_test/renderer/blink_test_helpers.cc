// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/blink_test_helpers.h"

#include <string_view>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/web_test/common/web_test_switches.h"
#include "content/web_test/renderer/test_preferences.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/display/display.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#endif

using blink::WebURL;

namespace {

constexpr std::string_view kFileScheme = "file:///";

base::FilePath GetWebTestsFilePath() {
  static base::FilePath path;
  if (path.empty()) {
    base::FilePath root_path;
    bool success =
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path);
    CHECK(success);
    path = root_path.Append(FILE_PATH_LITERAL("third_party/blink/web_tests/"));
  }
  return path;
}

base::FilePath GetExternalWPTFilePath() {
  static base::FilePath path;
  if (path.empty()) {
    base::FilePath root_path;
    bool success =
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path);
    CHECK(success);
    path = root_path.Append(
        FILE_PATH_LITERAL("third_party/blink/web_tests/external/wpt"));
  }
  return path;
}

// WPT tests use absolute path links such as
//   <script src="/resources/testharness.js">.
// If we load the tests as local files (e.g. when we run
// `content_shell --run-web-tests manually for testing or debugging), such
// links don't work. This function fixes this issue by rewriting file: URLs
// which were produced from such links so that they point actual files under
// the WPT test directory.
//
// Note that this doesn't apply when the WPT tests are run by the python script.
WebURL RewriteWPTAbsolutePath(std::string_view utf8_url) {
  if (!base::StartsWith(utf8_url, kFileScheme, base::CompareCase::SENSITIVE) ||
      utf8_url.find("/web_tests/") != std::string::npos) {
    return WebURL(GURL(utf8_url));
  }

#if BUILDFLAG(IS_WIN)
  // +3 for a drive letter, :, and /.
  static constexpr size_t kFileSchemeAndDriveLen = kFileScheme.size() + 3;
  if (utf8_url.size() <= kFileSchemeAndDriveLen)
    return WebURL();
  std::string_view path = utf8_url.substr(kFileSchemeAndDriveLen);
#else
  std::string_view path = utf8_url.substr(kFileScheme.size());
#endif
  base::FilePath new_path = GetExternalWPTFilePath().AppendASCII(path);
  return WebURL(net::FilePathToFileURL(new_path));
}

}  // namespace

namespace content {

void ExportWebTestSpecificPreferences(const TestPreferences& from,
                                      blink::web_pref::WebPreferences* to) {
  to->javascript_can_access_clipboard = from.java_script_can_access_clipboard;
  to->editing_behavior = from.editing_behavior;
  to->default_font_size = from.default_font_size;
  to->minimum_font_size = from.minimum_font_size;
  to->default_encoding = from.default_text_encoding_name.Utf8().data();
  to->javascript_enabled = from.java_script_enabled;
  to->supports_multiple_windows = from.supports_multiple_windows;
  to->loads_images_automatically = from.loads_images_automatically;
  to->plugins_enabled = from.plugins_enabled;
  to->tabs_to_links = from.tabs_to_links;
  // experimentalCSSRegionsEnabled is deprecated and ignored.
  to->hyperlink_auditing_enabled = from.hyperlink_auditing_enabled;
  to->allow_running_insecure_content = from.allow_running_of_insecure_content;
  to->allow_file_access_from_file_urls = from.allow_file_access_from_file_urls;
  to->web_security_enabled = from.web_security_enabled;
  to->disable_reading_from_canvas = from.disable_reading_from_canvas;
  to->strict_mixed_content_checking = from.strict_mixed_content_checking;
  to->strict_powerful_feature_restrictions =
      from.strict_powerful_feature_restrictions;
  to->spatial_navigation_enabled = from.spatial_navigation_enabled;
}

static base::FilePath GetBuildDirectory() {
#if BUILDFLAG(IS_MAC)
  if (base::apple::AmIBundled()) {
    // If this is a bundled Content Shell.app, go up one from the outer bundle
    // directory.
    return base::apple::OuterBundlePath().DirName();
  }
#endif

  base::FilePath result;
  bool success = base::PathService::Get(base::DIR_EXE, &result);
  CHECK(success);

  return result;
}

WebURL RewriteWebTestsURL(std::string_view utf8_url, bool is_wpt_mode) {
  if (is_wpt_mode)
    return RewriteWPTAbsolutePath(utf8_url);

  static constexpr std::string_view kGenPrefix = "file:///gen/";

  // Map "file:///gen/" to "file://<build directory>/gen/".
  if (base::StartsWith(utf8_url, kGenPrefix, base::CompareCase::SENSITIVE)) {
    base::FilePath gen_directory_path =
        GetBuildDirectory().Append(FILE_PATH_LITERAL("gen/"));
    std::string new_url("file://");
    new_url.append(gen_directory_path.AsUTF8Unsafe());
    new_url.append(utf8_url.substr(kGenPrefix.size()));
    return WebURL(GURL(new_url));
  }

  static constexpr std::string_view kPrefix = "file:///tmp/web_tests/";

  if (!base::StartsWith(utf8_url, kPrefix, base::CompareCase::SENSITIVE))
    return WebURL(GURL(utf8_url));

  std::string new_url("file://");
  new_url.append(GetWebTestsFilePath().AsUTF8Unsafe());
  new_url.append(utf8_url.substr(kPrefix.size()));
  return WebURL(GURL(new_url));
}

WebURL RewriteFileURLToLocalResource(std::string_view resource) {
  return RewriteWebTestsURL(resource, /*is_wpt_mode=*/false);
}

bool IsWebPlatformTest(std::string_view test_url) {
  // ://web-platform.test is a part of the http/https URL of a wpt test run by
  // the python script.
  return test_url.find("://web-platform.test") != std::string::npos ||
         // These are part of the file URL of a wpt test run manually with
         // content_shell without a web server.
         test_url.find("/external/wpt/") != std::string::npos ||
         test_url.find("/wpt_internal/") != std::string::npos;
}

}  // namespace content
