// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/common/javascript_dialog_type.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHost;
class WebContents;

// An interface consisting of methods that can be called to produce and manage
// JavaScript dialogs.
class CONTENT_EXPORT JavaScriptDialogManager {
 public:
  using DialogClosedCallback =
      base::OnceCallback<void(bool /* success */,
                              const std::u16string& /* user_input */)>;

  // Displays a JavaScript dialog. |did_suppress_message| will not be nil; if
  // |true| is returned in it, the caller will handle faking the reply.
  virtual void RunJavaScriptDialog(WebContents* web_contents,
                                   RenderFrameHost* render_frame_host,
                                   JavaScriptDialogType dialog_type,
                                   const std::u16string& message_text,
                                   const std::u16string& default_prompt_text,
                                   DialogClosedCallback callback,
                                   bool* did_suppress_message) = 0;

  // Displays a dialog asking the user if they want to leave a page.
  virtual void RunBeforeUnloadDialog(WebContents* web_contents,
                                     RenderFrameHost* render_frame_host,
                                     bool is_reload,
                                     DialogClosedCallback callback) = 0;

  // Accepts or dismisses the active JavaScript dialog, which must be owned
  // by the given |web_contents|. If |prompt_override| is not null, the prompt
  // text of the dialog should be set before accepting. Returns true if the
  // dialog was handled.
  virtual bool HandleJavaScriptDialog(WebContents* web_contents,
                                      bool accept,
                                      const std::u16string* prompt_override);

  // Cancels all active and pending dialogs for the given WebContents. If
  // |reset_state| is true, resets any saved state tied to |web_contents|.
  virtual void CancelDialogs(WebContents* web_contents,
                             bool reset_state) = 0;

  virtual ~JavaScriptDialogManager() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
