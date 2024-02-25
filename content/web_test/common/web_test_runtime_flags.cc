// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/common/web_test_runtime_flags.h"

namespace content {

WebTestRuntimeFlags::WebTestRuntimeFlags() {
  Reset();
}

void WebTestRuntimeFlags::Reset() {
  set_generate_pixel_results(true);

  set_dump_as_text(false);
  set_dump_as_markup(false);
  set_dump_as_layout(false);
  set_dump_child_frames(false);

  set_is_printing(false);
  set_printing_frame("");
  set_printing_width(0);
  set_printing_height(0);
  set_printing_margin(0);

  set_policy_delegate_enabled(false);
  set_policy_delegate_is_permissive(false);
  set_policy_delegate_should_notify_done(false);
  set_wait_until_done(false);
  set_wait_until_external_url_load(false);

  set_dump_selection_rect(false);
  set_dump_drag_image(false);

  set_dump_web_content_settings_client_callbacks(false);
  set_storage_allowed(true);
  set_running_insecure_content_allowed(false);

  set_dump_editting_callbacks(false);
  set_dump_frame_load_callbacks(false);
  set_dump_ping_loader_callbacks(false);
  set_dump_user_gesture_in_frame_load_callbacks(false);
  set_dump_resource_load_callbacks(false);
  set_dump_navigation_policy(false);

  set_dump_title_changes(false);
  set_dump_icon_changes(false);
  set_dump_console_messages(true);

  set_stay_on_page_after_handling_before_unload(false);

  set_have_loading_frame(false);

  set_dump_javascript_dialogs(true);

  set_has_custom_text_output(false);
  set_custom_text_output("");

  set_is_web_platform_tests_mode(false);

  // No need to report the initial state - only the future delta is important.
  tracked_dictionary().ResetChangeTracking();

  set_auto_drag_drop_enabled(true);
}

}  // namespace content
