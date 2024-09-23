// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_IOS_H_
#define IOS_WEB_CONTENT_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_IOS_H_

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "components/javascript_dialogs/tab_modal_dialog_manager_delegate.h"

namespace content {
class WebContents;
}

class JavaScriptTabModalDialogManagerDelegateIOS
    : public javascript_dialogs::TabModalDialogManagerDelegate {
 public:
  explicit JavaScriptTabModalDialogManagerDelegateIOS(
      content::WebContents* web_contents);
  ~JavaScriptTabModalDialogManagerDelegateIOS() override;

  JavaScriptTabModalDialogManagerDelegateIOS(
      const JavaScriptTabModalDialogManagerDelegateIOS& other) = delete;
  JavaScriptTabModalDialogManagerDelegateIOS& operator=(
      const JavaScriptTabModalDialogManagerDelegateIOS& other) = delete;

  // javascript_dialogs::TabModalDialogManagerDelegate:
  base::WeakPtr<javascript_dialogs::TabModalDialogView> CreateNewDialog(
      content::WebContents* alerting_web_contents,
      const std::u16string& title,
      content::JavaScriptDialogType dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_closed_callback) override;
  void WillRunDialog() override;
  void DidCloseDialog() override;
  void SetTabNeedsAttention(bool attention) override;
  bool IsWebContentsForemost() override;
  bool IsApp() override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // IOS_WEB_CONTENT_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_IOS_H_
