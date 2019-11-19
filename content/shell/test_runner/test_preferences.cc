// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/test_preferences.h"

#include "build/build_config.h"

using blink::WebSettings;
using blink::WebString;

namespace test_runner {

TestPreferences::TestPreferences() {
  Reset();
}

void TestPreferences::Reset() {
  default_font_size = 16;
  minimum_font_size = 0;
  allow_file_access_from_file_urls = true;
  allow_running_of_insecure_content = true;
  default_text_encoding_name = WebString::FromUTF8("ISO-8859-1");
  experimental_css_grid_layout_enabled = true;
  java_script_can_access_clipboard = true;
  supports_multiple_windows = true;
  java_script_enabled = true;
  loads_images_automatically = true;
  plugins_enabled = true;
  caret_browsing_enabled = false;
  allow_universal_access_from_file_urls = false;

#if defined(OS_MACOSX)
  editing_behavior = WebSettings::EditingBehavior::kMac;
#else
  editing_behavior = WebSettings::EditingBehavior::kWin;
#endif

  tabs_to_links = false;
  hyperlink_auditing_enabled = false;
  should_respect_image_orientation = false;
  asynchronous_spell_checking_enabled = false;
  web_security_enabled = true;
  disable_reading_from_canvas = false;
  strict_mixed_content_checking = false;
  strict_powerful_feature_restrictions = false;
  spatial_navigation_enabled = false;
}

}  // namespace test_runner
