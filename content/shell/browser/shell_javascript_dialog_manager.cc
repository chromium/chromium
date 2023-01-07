// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_javascript_dialog_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell_javascript_dialog.h"
#include "content/shell/common/shell_switches.h"

namespace content {

ShellJavaScriptDialogManager::ShellJavaScriptDialogManager()
    : should_proceed_on_beforeunload_(true), beforeunload_success_(true) {}

ShellJavaScriptDialogManager::~ShellJavaScriptDialogManager() = default;

void ShellJavaScriptDialogManager::RunJavaScriptDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  if (dialog_request_callback_) {
    std::move(dialog_request_callback_).Run();
    std::move(callback).Run(true, std::u16string());
    return;
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  *did_suppress_message = false;

  if (dialog_) {
    // One dialog at a time, please.
    *did_suppress_message = true;
    return;
  }

  std::u16string new_message_text =
      url_formatter::FormatUrl(
          render_frame_host->GetLastCommittedOrigin().GetURL()) +
      u"\n\n" + message_text;
  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();

  dialog_ = std::make_unique<ShellJavaScriptDialog>(
      this, parent_window, dialog_type, new_message_text, default_prompt_text,
      std::move(callback));
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
  *did_suppress_message = true;
  return;
#endif
}

void ShellJavaScriptDialogManager::RunBeforeUnloadDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  // During tests, if the BeforeUnload should not proceed automatically, store
  // the callback and return.
  if (dialog_request_callback_) {
    std::move(dialog_request_callback_).Run();

    if (should_proceed_on_beforeunload_)
      std::move(callback).Run(beforeunload_success_, std::u16string());
    else
      before_unload_callback_ = std::move(callback);
    return;
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (dialog_) {
    // Seriously!?
    std::move(callback).Run(true, std::u16string());
    return;
  }

  std::u16string message_text = u"Is it OK to leave/reload this page?";

  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();

  dialog_ = std::make_unique<ShellJavaScriptDialog>(
      this, parent_window, JAVASCRIPT_DIALOG_TYPE_CONFIRM, message_text,
      std::u16string(),  // default
      std::move(callback));
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
  std::move(callback).Run(true, std::u16string());
  return;
#endif
}

void ShellJavaScriptDialogManager::CancelDialogs(WebContents* web_contents,
                                                 bool reset_state) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (dialog_) {
    dialog_->Cancel();
    dialog_.reset();
  }
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
#endif

  if (before_unload_callback_.is_null())
    return;

  if (reset_state)
    std::move(before_unload_callback_).Run(false, std::u16string());
}

void ShellJavaScriptDialogManager::DialogClosed(ShellJavaScriptDialog* dialog) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  DCHECK_EQ(dialog, dialog_.get());
  dialog_.reset();
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
#endif
}

}  // namespace content
