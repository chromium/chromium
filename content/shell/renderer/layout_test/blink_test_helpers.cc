// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/blink_test_helpers.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/web_preferences.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/test_runner/test_preferences.h"
#include "net/base/filename_util.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

using blink::WebURL;

namespace {

base::FilePath GetWebTestsFilePath() {
  static base::FilePath path;
  if (path.empty()) {
    base::FilePath root_path;
    bool success = base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path);
    CHECK(success);
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kTestsInBlink)) {
      path =
          root_path.Append(FILE_PATH_LITERAL("third_party/blink/web_tests/"));
    } else {
      path = root_path.Append(
          FILE_PATH_LITERAL("third_party/WebKit/LayoutTests/"));
    }
  }
  return path;
}

// Tests in csswg-test use absolute path links such as
//   <script src="/resources/testharness.js">.
// Because we load the tests as local files, such links don't work.
// This function fixes this issue by rewriting file: URLs which were produced
// from such links so that they point actual files in LayoutTests/resources/.
//
// Note that this isn't applied to external/wpt because tests in external/wpt
// are accessed via http.
WebURL RewriteAbsolutePathInCsswgTest(const std::string& utf8_url) {
  static constexpr base::StringPiece kFileScheme = "file:///";
  if (!base::StartsWith(utf8_url, kFileScheme, base::CompareCase::SENSITIVE))
    return WebURL();
  if (utf8_url.find("/LayoutTests/") != std::string::npos ||
      utf8_url.find("/web_tests/") != std::string::npos)
    return WebURL();
#if defined(OS_WIN)
  // +3 for a drive letter, :, and /.
  static constexpr size_t kFileSchemeAndDriveLen = kFileScheme.size() + 3;
  if (utf8_url.size() <= kFileSchemeAndDriveLen)
    return WebURL();
  std::string path = utf8_url.substr(kFileSchemeAndDriveLen);
#else
  std::string path = utf8_url.substr(kFileScheme.size());
#endif
  base::FilePath new_path = GetWebTestsFilePath().AppendASCII(path);
  return WebURL(net::FilePathToFileURL(new_path));
}

}  // namespace

namespace content {

void ExportLayoutTestSpecificPreferences(
    const test_runner::TestPreferences& from,
    WebPreferences* to) {
  to->allow_universal_access_from_file_urls =
      from.allow_universal_access_from_file_urls;
  to->javascript_can_access_clipboard = from.java_script_can_access_clipboard;
  to->xss_auditor_enabled = from.xss_auditor_enabled;
  to->editing_behavior = static_cast<EditingBehavior>(from.editing_behavior);
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
  to->should_respect_image_orientation = from.should_respect_image_orientation;
  to->allow_file_access_from_file_urls = from.allow_file_access_from_file_urls;
  to->web_security_enabled =
      from.web_security_enabled;
  to->disable_reading_from_canvas = from.disable_reading_from_canvas;
  to->strict_mixed_content_checking =
      from.strict_mixed_content_checking;
  to->strict_powerful_feature_restrictions =
      from.strict_powerful_feature_restrictions;
  to->spatial_navigation_enabled = from.spatial_navigation_enabled;
}

// Applies settings that differ between layout tests and regular mode. Some
// of the defaults are controlled via command line flags which are
// automatically set for layout tests.
void ApplyLayoutTestDefaultPreferences(WebPreferences* prefs) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  prefs->allow_universal_access_from_file_urls = true;
  prefs->dom_paste_enabled = true;
  prefs->javascript_can_access_clipboard = true;
  prefs->xslt_enabled = true;
  prefs->xss_auditor_enabled = false;
#if defined(OS_MACOSX)
  prefs->editing_behavior = EDITING_BEHAVIOR_MAC;
#else
  prefs->editing_behavior = EDITING_BEHAVIOR_WIN;
#endif
  prefs->application_cache_enabled = true;
  prefs->tabs_to_links = false;
  prefs->hyperlink_auditing_enabled = false;
  prefs->allow_running_insecure_content = false;
  prefs->disable_reading_from_canvas = false;
  prefs->strict_mixed_content_checking = false;
  prefs->strict_powerful_feature_restrictions = false;
  prefs->webgl_errors_to_console_enabled = false;
  base::string16 serif;
#if defined(OS_MACOSX)
  prefs->cursive_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Apple Chancery");
  prefs->fantasy_font_family_map[kCommonScript] = base::ASCIIToUTF16("Papyrus");
  serif = base::ASCIIToUTF16("Times");
#else
  prefs->cursive_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Comic Sans MS");
  prefs->fantasy_font_family_map[kCommonScript] = base::ASCIIToUTF16("Impact");
  serif = base::ASCIIToUTF16("times new roman");
#endif
  prefs->serif_font_family_map[kCommonScript] = serif;
  prefs->standard_font_family_map[kCommonScript] = serif;
  prefs->fixed_font_family_map[kCommonScript] = base::ASCIIToUTF16("Courier");
  prefs->sans_serif_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Helvetica");
  prefs->minimum_logical_font_size = 9;
  prefs->accelerated_2d_canvas_enabled =
      command_line.HasSwitch(switches::kEnableAccelerated2DCanvas);
  prefs->mock_scrollbars_enabled = false;
  prefs->smart_insert_delete_enabled = true;
  prefs->minimum_accelerated_2d_canvas_size = 0;
#if defined(OS_ANDROID)
  prefs->text_autosizing_enabled = false;
#endif
  prefs->viewport_enabled = command_line.HasSwitch(switches::kEnableViewport);
  prefs->default_minimum_page_scale_factor = 1.f;
  prefs->default_maximum_page_scale_factor = 4.f;
  prefs->presentation_receiver =
      command_line.HasSwitch(switches::kForcePresentationReceiverForTesting);
  prefs->translate_service_available = true;
}

base::FilePath GetBuildDirectory() {
  base::FilePath result;
  bool success = base::PathService::Get(base::DIR_EXE, &result);
  CHECK(success);

#if defined(OS_MACOSX)
  if (base::mac::AmIBundled()) {
    // The bundled app executables live three levels down from the build
    // directory, eg:
    // Content Shell.app/Contents/Frameworks/Content Shell Helper.app
    // And this helper executable lives an additional three levels down:
    // Content Shell Helper.app/Contents/MacOS/Content Shell Helper
    result = result.DirName().DirName().DirName().DirName().DirName().DirName();
  }
#endif
  return result;
}

WebURL RewriteLayoutTestsURL(const std::string& utf8_url, bool is_wpt_mode) {
  if (is_wpt_mode) {
    WebURL rewritten_url = RewriteAbsolutePathInCsswgTest(utf8_url);
    if (!rewritten_url.IsEmpty())
      return rewritten_url;
    return WebURL(GURL(utf8_url));
  }

  static constexpr base::StringPiece kGenPrefix = "file:///gen/";

  // Map "file:///gen/" to "file://<build directory>/gen/".
  if (base::StartsWith(utf8_url, kGenPrefix, base::CompareCase::SENSITIVE)) {
    base::FilePath gen_directory_path =
        GetBuildDirectory().Append(FILE_PATH_LITERAL("gen/"));
    std::string new_url = std::string("file://") +
                          gen_directory_path.AsUTF8Unsafe() +
                          utf8_url.substr(kGenPrefix.size());
    return WebURL(GURL(new_url));
  }

  // TODO(tkent): Replace "tmp/LayoutTests" in tests with "tmp/web_tests".
  static constexpr base::StringPiece kPrefix = "file:///tmp/LayoutTests/";

  if (!base::StartsWith(utf8_url, kPrefix, base::CompareCase::SENSITIVE))
    return WebURL(GURL(utf8_url));

  std::string utf8_path = GetWebTestsFilePath().AsUTF8Unsafe();
  std::string new_url =
      std::string("file://") + utf8_path + utf8_url.substr(kPrefix.size());
  return WebURL(GURL(new_url));
}

}  // namespace content
