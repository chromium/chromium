// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_TEST_PREFERENCES_H_
#define CONTENT_WEB_TEST_RENDERER_TEST_PREFERENCES_H_

#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

namespace content {

struct TestPreferences {
  int default_font_size;
  int minimum_font_size;
  bool allow_file_access_from_file_urls;
  bool allow_running_of_insecure_content;
  blink::WebString default_text_encoding_name;
  bool experimental_css_grid_layout_enabled;
  bool java_script_can_access_clipboard;
  bool supports_multiple_windows;
  bool java_script_enabled;
  bool loads_images_automatically;
  bool plugins_enabled;
  bool allow_universal_access_from_file_urls;
  blink::mojom::EditingBehavior editing_behavior;
  bool tabs_to_links;
  bool hyperlink_auditing_enabled;
  bool caret_browsing_enabled;
  bool asynchronous_spell_checking_enabled;
  bool web_security_enabled;
  bool disable_reading_from_canvas;
  bool strict_mixed_content_checking;
  bool strict_powerful_feature_restrictions;
  bool spatial_navigation_enabled;

  TestPreferences();
  void Reset();
};
}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_TEST_PREFERENCES_H_
