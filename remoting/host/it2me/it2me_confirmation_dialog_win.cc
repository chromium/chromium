// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <commctrl.h>

#include <cstdint>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/win/core_resource.h"

namespace remoting {

namespace {

// Time to wait before closing the dialog and cancelling the connection.
constexpr base::TimeDelta kDialogTimeout = base::TimeDelta::FromMinutes(1);

const HRESULT kTimeoutErrorCode = E_ABORT;

// Loads an embedded string resource from the specified module.
bool LoadStringResource(HMODULE resource_module,
                        int resource_id,
                        std::wstring* string) {
  DCHECK(resource_module);
  DCHECK(string);

  string->clear();

  const wchar_t* string_resource = nullptr;
  int string_length = LoadStringW(resource_module, resource_id,
                                  reinterpret_cast<wchar_t*>(&string_resource),
                                  /*nBufferMax=*/0);
  if (string_length <= 0) {
    PLOG(ERROR) << "LoadStringW() failed";
    return false;
  }

  string->append(string_resource, string_length);
  return true;
}

class It2MeConfirmationDialogWin : public It2MeConfirmationDialog {
 public:
  It2MeConfirmationDialogWin();
  ~It2MeConfirmationDialogWin() override;

  static HRESULT CALLBACK TaskDialogCallbackProc(HWND hwnd,
                                                 UINT notification,
                                                 WPARAM w_param,
                                                 LPARAM l_param,
                                                 LONG_PTR ref_data);

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            ResultCallback callback) override;

 private:
  // Tracks whether the dialog was in the foreground the last time we checked.
  // Default to true so we will attempt to bring it back if it starts in the
  // background for some reason.
  bool is_foreground_window_ = true;

  DISALLOW_COPY_AND_ASSIGN(It2MeConfirmationDialogWin);
};

It2MeConfirmationDialogWin::It2MeConfirmationDialogWin() {}

It2MeConfirmationDialogWin::~It2MeConfirmationDialogWin() {}

void It2MeConfirmationDialogWin::Show(const std::string& remote_user_email,
                                      ResultCallback callback) {
  DCHECK(!remote_user_email.empty());
  DCHECK(callback);

  // Default to a cancelled state.  We only accept the connection if the user
  // explicitly allows it.
  Result result = Result::CANCEL;

  // |resource_module| does not need to be freed as GetModuleHandle() does not
  // increment the refcount for the module.  This DLL is not unloaded until the
  // process exits so using a stored handle is safe.
  HMODULE resource_module = GetModuleHandle(L"remoting_core.dll");
  if (resource_module == nullptr) {
    PLOG(ERROR) << "GetModuleHandle() failed";
    std::move(callback).Run(result);
    return;
  }

  std::wstring title_text;
  if (!LoadStringResource(resource_module, IDS_PRODUCT_NAME, &title_text)) {
    LOG(ERROR) << "Failed to load title text for confirmation dialog.";
    std::move(callback).Run(result);
    return;
  }

  std::wstring message_text;
  if (!LoadStringResource(resource_module,
                          IDS_SHARE_CONFIRM_DIALOG_MESSAGE_WITH_USERNAME,
                          &message_text)) {
    LOG(ERROR) << "Failed to load message text for confirmation dialog.";
    std::move(callback).Run(result);
    return;
  }
  message_text =
      base::AsWString(base::i18n::MessageFormatter::FormatWithNumberedArgs(
          base::AsStringPiece16(message_text),
          base::UTF8ToUTF16(remote_user_email)));

  std::wstring share_button_text;
  if (!LoadStringResource(resource_module, IDS_SHARE_CONFIRM_DIALOG_CONFIRM,
                          &share_button_text)) {
    LOG(ERROR) << "Failed to load share button text for confirmation dialog.";
    std::move(callback).Run(result);
    return;
  }

  std::wstring decline_button_text;
  if (!LoadStringResource(resource_module, IDS_SHARE_CONFIRM_DIALOG_DECLINE,
                          &decline_button_text)) {
    LOG(ERROR) << "Failed to load decline button text for confirmation dialog.";
    std::move(callback).Run(result);
    return;
  }

  TASKDIALOG_BUTTON dialog_buttons[] = {
      {IDYES, share_button_text.c_str()}, {IDNO, decline_button_text.c_str()},
  };

  TASKDIALOGCONFIG dialog_config = {0};
  dialog_config.cbSize = sizeof(dialog_config);
  dialog_config.hInstance = resource_module;
  dialog_config.pszWindowTitle = title_text.c_str();
  dialog_config.pszMainInstruction = message_text.c_str();
  dialog_config.pszMainIcon = MAKEINTRESOURCE(IDI_CHROME_REMOTE_DESKTOP);
  dialog_config.dwFlags = TDF_CALLBACK_TIMER;
  dialog_config.pfCallback = &TaskDialogCallbackProc;
  dialog_config.lpCallbackData = reinterpret_cast<LONG_PTR>(this);
  dialog_config.cButtons = ARRAYSIZE(dialog_buttons);
  dialog_config.pButtons = dialog_buttons;
  dialog_config.nDefaultButton = IDNO;

  int button_result = 0;
  HRESULT hr = TaskDialogIndirect(&dialog_config, &button_result,
                                  /*pnRadioButton=*/nullptr,
                                  /*pfVerificationFlagChecked=*/nullptr);
  if (FAILED(hr)) {
    if (hr == kTimeoutErrorCode) {
      LOG(INFO) << "TaskDialog timed out.";
    } else {
      LOG(ERROR) << "TaskDialogIndirect() Failed: 0x" << std::hex << hr;
    }

    std::move(callback).Run(result);
    return;
  }

  if (button_result == IDYES) {
    // Only accept the connection if the user chose 'share'.
    result = Result::OK;
  }

  std::move(callback).Run(result);
}

HRESULT CALLBACK
It2MeConfirmationDialogWin::TaskDialogCallbackProc(HWND hwnd,
                                                   UINT notification,
                                                   WPARAM w_param,
                                                   LPARAM l_param,
                                                   LONG_PTR ref_data) {
  if (notification == TDN_TIMER) {
    if (static_cast<int64_t>(w_param) >= kDialogTimeout.InMilliseconds()) {
      // Close the dialog window if we have reached the timeout.
      return kTimeoutErrorCode;
    }

    // Ensure the window is visible before checking if it is in the foreground.
    if (!IsWindowVisible(hwnd)) {
      ShowWindow(hwnd, SW_SHOWNORMAL);
    }

    // Attempt to bring the dialog window to the foreground if needed.  If the
    // window is in the background and cannot be brought forward, this call will
    // flash the placeholder on the taskbar.  Do not call SetForegroundWindow()
    // multiple times as it will cause annoying flashing for the user.
    It2MeConfirmationDialogWin* dialog =
        reinterpret_cast<It2MeConfirmationDialogWin*>(ref_data);
    if (hwnd == GetForegroundWindow()) {
      dialog->is_foreground_window_ = true;
    } else if (dialog->is_foreground_window_) {
      SetForegroundWindow(hwnd);
      dialog->is_foreground_window_ = false;
    }

    if (!dialog->is_foreground_window_) {
      // Ensure the dialog is always at the top of the top-most window stack,
      // even if it doesn't have focus, so the user can always see it.
      BringWindowToTop(hwnd);
    }
  } else if (notification == TDN_CREATED) {
    // After the dialog has been created, but before it is visible, set its
    // z-order so it will be a top-most window and have always on top behavior.
    if (!SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE)) {
      PLOG(ERROR) << "SetWindowPos() failed";
    }
  }

  return S_OK;
}

}  // namespace

std::unique_ptr<It2MeConfirmationDialog>
It2MeConfirmationDialogFactory::Create() {
  return std::make_unique<It2MeConfirmationDialogWin>();
}

}  // namespace remoting
