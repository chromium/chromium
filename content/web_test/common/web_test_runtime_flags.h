// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_COMMON_WEB_TEST_RUNTIME_FLAGS_H_
#define CONTENT_WEB_TEST_COMMON_WEB_TEST_RUNTIME_FLAGS_H_

#include <string>

#include "base/check.h"
#include "content/web_test/common/tracked_dictionary.h"

namespace content {

// WebTestRuntimeFlags stores flags controlled by web tests at runtime
// (i.e. by calling testRunner.dumpAsText() or testRunner.waitUntilDone()).
// Changes to the flags are tracked (to help replicate them across renderers).
class WebTestRuntimeFlags {
 public:
  // Creates default flags (see also the Reset method).
  WebTestRuntimeFlags();

  WebTestRuntimeFlags(const WebTestRuntimeFlags&) = delete;
  WebTestRuntimeFlags& operator=(const WebTestRuntimeFlags&) = delete;

  // Resets all the values to their defaults.
  void Reset();

  TrackedDictionary& tracked_dictionary() { return dict_; }

#define DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(name)             \
  bool name() const {                                       \
    std::optional<bool> result =                            \
        dict_.current_values().FindBoolByDottedPath(#name); \
    DCHECK(result);                                         \
    return *result;                                         \
  }                                                         \
  void set_##name(bool new_value) { dict_.SetBoolean(#name, new_value); }

#define DEFINE_INT_WEB_TEST_RUNTIME_FLAG(name)             \
  int name() const {                                       \
    std::optional<int> result =                            \
        dict_.current_values().FindIntByDottedPath(#name); \
    DCHECK(result);                                        \
    return *result;                                        \
  }                                                        \
  void set_##name(int new_value) { dict_.SetInteger(#name, new_value); }

#define DEFINE_STRING_WEB_TEST_RUNTIME_FLAG(name)             \
  std::string name() const {                                  \
    const std::string* result =                               \
        dict_.current_values().FindStringByDottedPath(#name); \
    DCHECK(result);                                           \
    return *result;                                           \
  }                                                           \
  void set_##name(const std::string& new_value) {             \
    dict_.SetString(#name, new_value);                        \
  }

  // If true, the test runner will generate pixel results.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(generate_pixel_results)

  // If true, the test runner will produce a plain text dump.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_as_text)

  // If true and dump_as_text is false, the test runner will produce a dump of
  // the DOM.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_as_markup)

  // If true and both dump_as_text and dump_as_markup are false, the test runner
  // will dump a text representation of the layout.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_as_layout)

  // If true, the test runner will recursively dump all frames as text, markup
  // or layout depending on which of dump_as_text, dump_as_markup and
  // dump_as_layout is effective.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_child_frames)

  // If true, layout is to target printed pages.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(is_printing)

  // If this is non-empty and is_printing is true, pixel dump will be for the
  // named frame printed.
  DEFINE_STRING_WEB_TEST_RUNTIME_FLAG(printing_frame)

  // Default page width and height when printing. Set both to 0 in order to use
  // the frame width / height.
  DEFINE_INT_WEB_TEST_RUNTIME_FLAG(printing_width)
  DEFINE_INT_WEB_TEST_RUNTIME_FLAG(printing_height)

  // Default page margin size when printing. This default margin will apply to
  // all four sides of the page.
  DEFINE_INT_WEB_TEST_RUNTIME_FLAG(printing_margin)

  // If true, don't dump output until notifyDone is called.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(wait_until_done)

  // If true, ends the test when a URL is loaded externally via
  // WebLocalFrameClient::loadURLExternally().
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(wait_until_external_url_load)

  // Causes navigation actions just printout the intended navigation instead
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(policy_delegate_enabled)

  // Toggles the behavior of the policy delegate. If true, then navigations
  // will be allowed. Otherwise, they will be ignored (dropped).
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(policy_delegate_is_permissive)

  // If true, the policy delegate will signal web test completion.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(policy_delegate_should_notify_done)

  // If true, the test runner will draw the bounds of the current selection rect
  // taking possible transforms of the selection rect into account.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_selection_rect)

  // If true, the test runner will dump the drag image as pixel results.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_drag_image)

  // Flags influencing behavior of WebTestContentSettingsClient.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(storage_allowed)
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(running_insecure_content_allowed)
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_web_content_settings_client_callbacks)

  // If true, the test runner will write a descriptive line for each editing
  // command.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_editting_callbacks)

  // If true, the test runner will output a descriptive line for each frame
  // load callback.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_frame_load_callbacks)

  // If true, the test runner will output a descriptive line for each
  // PingLoader dispatched.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_ping_loader_callbacks)

  // If true, the test runner will output a line of the user gesture status
  // text for some frame load callbacks.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_user_gesture_in_frame_load_callbacks)

  // If true, the test runner will output a descriptive line for each resource
  // load callback.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_resource_load_callbacks)

  // If true, content_shell will dump the default navigation policy passed to
  // WebLocalFrameClient::decidePolicyForNavigation.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_navigation_policy)

  // If true, output a message when the page title is changed.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_title_changes)

  // If true, the test runner will print out the icon change notifications.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_icon_changes)

  // If true, the console messages produced by the page will
  // be part of test output.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_console_messages)

  // Desired return value of WebLocalFrameClient::runModalBeforeUnloadDialog.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(stay_on_page_after_handling_before_unload)

  // Indicates if one renderer process is in charge of tracking the loading
  // frames. Only one can do it at a time.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(have_loading_frame)

  // If true, content_shell will output text for alert(), confirm(), prompt(),
  // etc.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(dump_javascript_dialogs)

  // True if the test called testRunner.setCustomTextOutput.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(has_custom_text_output)

  // Contains text passed by the test to testRunner.setCustomTextOutput.
  DEFINE_STRING_WEB_TEST_RUNTIME_FLAG(custom_text_output)

  // True for web platform tests.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(is_web_platform_tests_mode)

  // Whether to enable automatic drag n' drop.
  DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG(auto_drag_drop_enabled)

#undef DEFINE_BOOL_WEB_TEST_RUNTIME_FLAG
#undef DEFINE_STRING_WEB_TEST_RUNTIME_FLAG

 private:
  TrackedDictionary dict_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_COMMON_WEB_TEST_RUNTIME_FLAGS_H_
