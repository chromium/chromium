// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_MANAGER_H_
#define CONTENT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace content {

class ShellJavaScriptDialog;

class ShellJavaScriptDialogManager : public JavaScriptDialogManager {
 public:
  ShellJavaScriptDialogManager();
  ~ShellJavaScriptDialogManager() override;

  // JavaScriptDialogManager:
  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
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
#if defined(OS_MACOSX) || defined(OS_WIN)
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

  DISALLOW_COPY_AND_ASSIGN(ShellJavaScriptDialogManager);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_MANAGER_H_
