// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_MANAGER_H_
#define CONTENT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace content {

class ShellJavaScriptDialog;

class ShellJavaScriptDialogManager : public JavaScriptDialogManager {
 public:
  ShellJavaScriptDialogManager();

  ShellJavaScriptDialogManager(const ShellJavaScriptDialogManager&) = delete;
  ShellJavaScriptDialogManager& operator=(const ShellJavaScriptDialogManager&) =
      delete;

  ~ShellJavaScriptDialogManager() override;

  // JavaScriptDialogManager:
  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;

  void CancelDialogs(WebContents* web_contents,
                     bool reset_state) override;

  // Called by the ShellJavaScriptDialog when it closes.
  void DialogClosed(ShellJavaScriptDialog* dialog);

  // Used for content_browsertests.
  void set_dialog_request_callback(base::OnceClosure callback) {
    dialog_request_callback_ = std::move(callback);
  }
  void set_should_proceed_on_beforeunload(bool proceed, bool success) {
    should_proceed_on_beforeunload_ = proceed;
    beforeunload_success_ = success;
  }

 private:
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // The dialog being shown. No queueing.
  std::unique_ptr<ShellJavaScriptDialog> dialog_;
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
#endif

  base::OnceClosure dialog_request_callback_;

  // Whether to automatically proceed when asked to display a BeforeUnload
  // dialog, and the return value that should be passed (success or failure).
  bool should_proceed_on_beforeunload_;
  bool beforeunload_success_;

  DialogClosedCallback before_unload_callback_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_MANAGER_H_
