// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_javascript_dialog.h"

#include <windows.h>

#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/shell/app/resource.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"

namespace content {

class ShellJavaScriptDialog;

INT_PTR CALLBACK ShellJavaScriptDialog::DialogProc(HWND dialog,
                                                   UINT message,
                                                   WPARAM wparam,
                                                   LPARAM lparam) {
  switch (message) {
    case WM_INITDIALOG: {
      SetWindowLongPtr(dialog, DWLP_USER, static_cast<LONG_PTR>(lparam));
      ShellJavaScriptDialog* owner =
          reinterpret_cast<ShellJavaScriptDialog*>(lparam);
      owner->dialog_win_ = dialog;
      SetDlgItemText(dialog, IDC_DIALOGTEXT,
                     base::as_wcstr(owner->message_text_));
      if (owner->dialog_type_ == JAVASCRIPT_DIALOG_TYPE_PROMPT)
        SetDlgItemText(dialog, IDC_PROMPTEDIT,
                       base::as_wcstr(owner->default_prompt_text_));
      break;
    }
    case WM_DESTROY: {
      ShellJavaScriptDialog* owner = reinterpret_cast<ShellJavaScriptDialog*>(
          GetWindowLongPtr(dialog, DWLP_USER));
      if (owner->dialog_win_) {
        owner->dialog_win_ = 0;
        std::move(owner->callback_).Run(false, std::u16string());
        owner->manager_->DialogClosed(owner);
      }
      break;
    }
    case WM_COMMAND: {
      ShellJavaScriptDialog* owner = reinterpret_cast<ShellJavaScriptDialog*>(
          GetWindowLongPtr(dialog, DWLP_USER));
      std::wstring user_input;
      bool finish = false;
      bool result = false;
      switch (LOWORD(wparam)) {
        case IDOK:
          finish = true;
          result = true;
          if (owner->dialog_type_ == JAVASCRIPT_DIALOG_TYPE_PROMPT) {
            int length =
                GetWindowTextLength(GetDlgItem(dialog, IDC_PROMPTEDIT)) + 1;
            GetDlgItemText(dialog, IDC_PROMPTEDIT,
                           base::WriteInto(&user_input, length), length);
          }
          break;
        case IDCANCEL:
          finish = true;
          result = false;
          break;
      }
      if (finish) {
        owner->dialog_win_ = 0;
        std::move(owner->callback_).Run(result, base::WideToUTF16(user_input));
        DestroyWindow(dialog);
        owner->manager_->DialogClosed(owner);
      }
      break;
    }
    default:
      return DefWindowProc(dialog, message, wparam, lparam);
  }
  return 0;
}

ShellJavaScriptDialog::ShellJavaScriptDialog(
    ShellJavaScriptDialogManager* manager,
    gfx::NativeWindow parent_window,
    JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    JavaScriptDialogManager::DialogClosedCallback callback)
    : callback_(std::move(callback)),
      manager_(manager),
      dialog_type_(dialog_type),
      message_text_(message_text),
      default_prompt_text_(default_prompt_text) {
  int dialog_resource;
  if (dialog_type == JAVASCRIPT_DIALOG_TYPE_ALERT)
    dialog_resource = IDD_ALERT;
  else if (dialog_type == JAVASCRIPT_DIALOG_TYPE_CONFIRM)
    dialog_resource = IDD_CONFIRM;
  else  // JAVASCRIPT_DIALOG_TYPE_PROMPT
    dialog_resource = IDD_PROMPT;

  dialog_win_ =
      CreateDialogParam(GetModuleHandle(0), MAKEINTRESOURCE(dialog_resource), 0,
                        DialogProc, reinterpret_cast<LPARAM>(this));
  ShowWindow(dialog_win_, SW_SHOWNORMAL);
}

ShellJavaScriptDialog::~ShellJavaScriptDialog() = default;

void ShellJavaScriptDialog::Cancel() {
  if (dialog_win_) {
    // DestroyWindow() will delete `this` as the WM_DESTROY event handler
    // deletes `this` through the `manager_`.
    DestroyWindow(dialog_win_);
  } else {
    // If the window failed to be created then we emulate WM_DESTROY, since
    // tests don't succeed in making dialogs always (e.g.
    // BackForwardCacheBrowserTest.CanUseCacheWhenPageAlertsInTimeoutLoop).
    std::move(callback_).Run(false, std::u16string());
    // DialogClosed() will delete `this`.
    manager_->DialogClosed(this);
  }
}

}  // namespace content
