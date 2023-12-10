// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/simple_task_dialog.h"

#include <iterator>
#include <string>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "remoting/host/win/core_resource.h"

namespace remoting {

namespace {

const HRESULT kTimeoutErrorCode = E_ABORT;

// Loads an embedded string resource from the specified module.
bool LoadStringResource(HMODULE resource_module,
                        int resource_id,
                        std::wstring& string) {
  DCHECK(resource_module);

  string.clear();

  const wchar_t* string_resource = nullptr;
  int string_length = LoadStringW(resource_module, resource_id,
                                  reinterpret_cast<wchar_t*>(&string_resource),
                                  /*cchBufferMax=*/0);
  if (string_length <= 0) {
    PLOG(ERROR) << "LoadStringW() failed for resource ID: " << resource_id;
    return false;
  }

  string.append(string_resource, string_length);
  return true;
}

}  // namespace

SimpleTaskDialog::SimpleTaskDialog(HMODULE resource_module)
    : resource_module_(resource_module) {}

SimpleTaskDialog::~SimpleTaskDialog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool SimpleTaskDialog::SetTitleTextWithStringId(int title_text_id) {
  return LoadStringResource(resource_module_, title_text_id, title_text_);
}

bool SimpleTaskDialog::SetMessageTextWithStringId(int message_text_id) {
  return LoadStringResource(resource_module_, message_text_id, message_text_);
}

void SimpleTaskDialog::AppendButton(int button_id,
                                    const std::wstring& button_text) {
  dialog_buttons_.emplace_back(button_id, button_text);
}

bool SimpleTaskDialog::AppendButtonWithStringId(int button_id,
                                                int button_text_id) {
  std::wstring button_text;
  if (!LoadStringResource(resource_module_, button_text_id, button_text)) {
    return false;
  }
  dialog_buttons_.emplace_back(button_id, button_text);
  return true;
}

std::optional<int> SimpleTaskDialog::Show() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<TASKDIALOG_BUTTON> taskdialog_buttons;
  base::ranges::transform(
      dialog_buttons_, std::back_inserter(taskdialog_buttons),
      [](const std::pair<int, std::wstring>& button) {
        return TASKDIALOG_BUTTON{button.first, button.second.c_str()};
      });

  TASKDIALOGCONFIG dialog_config = {0};
  dialog_config.cbSize = sizeof(dialog_config);
  dialog_config.hInstance = resource_module_;
  dialog_config.pszWindowTitle = title_text_.c_str();
  dialog_config.pszMainInstruction = message_text_.c_str();
  dialog_config.pszMainIcon = MAKEINTRESOURCE(IDI_CHROME_REMOTE_DESKTOP);
  dialog_config.dwFlags = TDF_CALLBACK_TIMER;
  dialog_config.pfCallback = &TaskDialogCallbackProc;
  dialog_config.lpCallbackData = reinterpret_cast<LONG_PTR>(this);
  dialog_config.cButtons = taskdialog_buttons.size();
  dialog_config.pButtons = taskdialog_buttons.data();
  dialog_config.nDefaultButton = default_button_;

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

    return std::nullopt;
  }
  return button_result;
}

// static
HRESULT CALLBACK SimpleTaskDialog::TaskDialogCallbackProc(HWND hwnd,
                                                          UINT notification,
                                                          WPARAM w_param,
                                                          LPARAM l_param,
                                                          LONG_PTR ref_data) {
  if (notification == TDN_TIMER) {
    SimpleTaskDialog* dialog = reinterpret_cast<SimpleTaskDialog*>(ref_data);

    DCHECK_CALLED_ON_VALID_SEQUENCE(dialog->sequence_checker_);

    if (!dialog->dialog_timeout_.is_zero() &&
        static_cast<int64_t>(w_param) >=
            dialog->dialog_timeout_.InMilliseconds()) {
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

}  // namespace remoting
