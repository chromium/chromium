// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_ios.h"

#import "components/javascript_dialogs/ios/tab_modal_dialog_view_ios.h"
#import "content/public/browser/javascript_dialog_manager.h"
#import "content/public/browser/visibility.h"
#import "content/public/browser/web_contents.h"

JavaScriptTabModalDialogManagerDelegateIOS::
    JavaScriptTabModalDialogManagerDelegateIOS(
        content::WebContents* web_contents)
    : web_contents_(web_contents) {}

JavaScriptTabModalDialogManagerDelegateIOS::
    ~JavaScriptTabModalDialogManagerDelegateIOS() = default;

base::WeakPtr<javascript_dialogs::TabModalDialogView>
JavaScriptTabModalDialogManagerDelegateIOS::CreateNewDialog(
    content::WebContents* alerting_web_contents,
    const std::u16string& title,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback
        callback_on_button_clicked,
    base::OnceClosure callback_on_cancelled) {
  return javascript_dialogs::TabModalDialogViewIOS::Create(
      web_contents_, alerting_web_contents, title, dialog_type, message_text,
      default_prompt_text, std::move(callback_on_button_clicked),
      std::move(callback_on_cancelled));
}

void JavaScriptTabModalDialogManagerDelegateIOS::WillRunDialog() {}

void JavaScriptTabModalDialogManagerDelegateIOS::DidCloseDialog() {}

void JavaScriptTabModalDialogManagerDelegateIOS::SetTabNeedsAttention(
    bool attention) {}

bool JavaScriptTabModalDialogManagerDelegateIOS::IsWebContentsForemost() {
  // TODO(crbug.com/361215210): Need to find a more effective way to determine
  // if the web content is active or in the foreground.
  return web_contents_->GetVisibility() == content::Visibility::VISIBLE;
}

bool JavaScriptTabModalDialogManagerDelegateIOS::IsApp() {
  return false;
}
