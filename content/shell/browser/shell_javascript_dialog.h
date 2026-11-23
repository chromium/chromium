// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_H_
#define CONTENT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "ui/gfx/native_ui_types.h"

namespace content {

class ShellJavaScriptDialogManager;

class ShellJavaScriptDialog {
 public:
  ShellJavaScriptDialog(ShellJavaScriptDialogManager* manager,
                        gfx::NativeWindow parent_window,
                        JavaScriptDialogType dialog_type,
                        const std::u16string& message_text,
                        const std::u16string& default_prompt_text,
                        JavaScriptDialogManager::DialogClosedCallback callback);

  ShellJavaScriptDialog(const ShellJavaScriptDialog&) = delete;
  ShellJavaScriptDialog& operator=(const ShellJavaScriptDialog&) = delete;

  ~ShellJavaScriptDialog();

  // Called to cancel a dialog mid-flight.
  void Cancel();

 private:
#if BUILDFLAG(IS_MAC)
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
#elif BUILDFLAG(IS_WIN)
  JavaScriptDialogManager::DialogClosedCallback callback_;
  raw_ptr<ShellJavaScriptDialogManager> manager_;
  JavaScriptDialogType dialog_type_;
  HWND dialog_win_;
  std::u16string message_text_;
  std::u16string default_prompt_text_;
  static INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wparam,
                                     LPARAM lparam);
#endif
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_H_
