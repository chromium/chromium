// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_JAVASCRIPT_DIALOG_MANAGER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_JAVASCRIPT_DIALOG_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"

namespace content {

class WebTestJavaScriptDialogManager : public ShellJavaScriptDialogManager {
 public:
  WebTestJavaScriptDialogManager();
  ~WebTestJavaScriptDialogManager() override;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTestJavaScriptDialogManager);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_JAVASCRIPT_DIALOG_MANAGER_H_
