// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_test_runtime_flags.h"

namespace test_runner {

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

  set_policy_delegate_enabled(false);
  set_policy_delegate_is_permissive(false);
  set_policy_delegate_should_notify_done(false);
  set_wait_until_done(false);
  set_wait_until_external_url_load(false);

  set_dump_selection_rect(false);
  set_dump_drag_image(false);

  set_accept_languages("");

  set_dump_web_content_settings_client_callbacks(false);
  set_images_allowed(true);
  set_scripts_allowed(true);
  set_storage_allowed(true);
  set_plugins_allowed(true);
  set_running_insecure_content_allowed(false);
  set_autoplay_allowed(true);

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

  set_can_open_windows(false);

  set_dump_create_view(false);
  set_dump_spell_check_callbacks(false);
  set_dump_javascript_dialogs(true);

  set_has_custom_text_output(false);
  set_custom_text_output("");

  // No need to report the initial state - only the future delta is important.
  tracked_dictionary().ResetChangeTracking();
}

}  // namespace test_runner
