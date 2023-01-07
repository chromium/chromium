// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_JAVASCRIPT_DIALOG_HELPER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_JAVASCRIPT_DIALOG_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace extensions {

class WebViewGuest;

class JavaScriptDialogHelper : public content::JavaScriptDialogManager {
 public:
  explicit JavaScriptDialogHelper(WebViewGuest* guest);

  JavaScriptDialogHelper(const JavaScriptDialogHelper&) = delete;
  JavaScriptDialogHelper& operator=(const JavaScriptDialogHelper&) = delete;

  ~JavaScriptDialogHelper() override;

  // JavaScriptDialogManager implementation.
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           content::RenderFrameHost* render_frame_host,
                           content::JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             content::RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const std::u16string* prompt_override) override;
  void CancelDialogs(content::WebContents* web_contents,
                     bool reset_state) override;

 private:
  void OnPermissionResponse(DialogClosedCallback callback,
                            bool allow,
                            const std::string& user_input);

  // Pointer to the webview that is being helped.
  const raw_ptr<WebViewGuest> web_view_guest_;

  base::WeakPtrFactory<JavaScriptDialogHelper> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_JAVASCRIPT_DIALOG_HELPER_H_
