// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_javascript_dialog_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell_javascript_dialog.h"
#include "content/shell/common/shell_switches.h"
#include "content/web_test/browser/web_test_control_host.h"

namespace content {

namespace {
bool DumpJavascriptDialog() {
  base::Optional<bool> result =
      WebTestControlHost::Get()
          ->accumulated_web_test_runtime_flags_changes()
          .FindBoolPath("dump_javascript_dialogs");
  if (!result.has_value())
    return true;
  return result.value();
}

bool ShouldStayOnPageAfterHandlingBeforeUnload() {
  base::Optional<bool> result =
      WebTestControlHost::Get()
          ->accumulated_web_test_runtime_flags_changes()
          .FindBoolPath("stay_on_page_after_handling_before_unload");
  if (!result.has_value())
    return false;
  return result.value();
}

}  // namespace

WebTestJavaScriptDialogManager::WebTestJavaScriptDialogManager() = default;

WebTestJavaScriptDialogManager::~WebTestJavaScriptDialogManager() = default;

void WebTestJavaScriptDialogManager::RunJavaScriptDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    JavaScriptDialogType dialog_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  if (DumpJavascriptDialog()) {
    std::string message;
    switch (dialog_type) {
      case JAVASCRIPT_DIALOG_TYPE_ALERT:
        message =
            base::StrCat({"ALERT: ", base::UTF16ToUTF8(message_text), "\n"});
        break;
      case JAVASCRIPT_DIALOG_TYPE_CONFIRM:
        message =
            base::StrCat({"CONFIRM: ", base::UTF16ToUTF8(message_text), "\n"});
        break;
      case JAVASCRIPT_DIALOG_TYPE_PROMPT:
        message = base::StrCat(
            {"PROMPT: ", base::UTF16ToUTF8(message_text),
             ", default text: ", base::UTF16ToUTF8(default_prompt_text), "\n"});
        break;
    }
    WebTestControlHost::Get()->printer()->AddMessageRaw(message);
  }
  std::move(callback).Run(true, base::string16());
}

void WebTestJavaScriptDialogManager::RunBeforeUnloadDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  if (DumpJavascriptDialog())
    WebTestControlHost::Get()->printer()->AddMessageRaw("CONFIRM NAVIGATION\n");
  std::move(callback).Run(!ShouldStayOnPageAfterHandlingBeforeUnload(),
                          base::string16());
}

}  // namespace content
