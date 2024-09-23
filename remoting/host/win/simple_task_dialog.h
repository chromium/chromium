// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_SIMPLE_TASK_DIALOG_H_
#define REMOTING_HOST_WIN_SIMPLE_TASK_DIALOG_H_

#include <windows.h>

#include <commctrl.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"

namespace remoting {

// A helper class to show a simple task dialog with a title, message, and a few
// buttons. The dialog will be kept on the top.
class SimpleTaskDialog final {
 public:
  explicit SimpleTaskDialog(HMODULE resource_module);
  ~SimpleTaskDialog();

  void set_title_text(const std::wstring& title_text) {
    title_text_ = title_text;
  }

  void set_message_text(const std::wstring& message_text) {
    message_text_ = message_text;
  }

  void set_default_button(int default_button) {
    default_button_ = default_button;
  }

  // Sets a timeout for the dialog. If the timeout is set and the dialog has
  // been shown longer than the timeout, it will be closed and Show() will
  // return nullopt.
  void set_dialog_timeout(base::TimeDelta dialog_timeout) {
    dialog_timeout_ = dialog_timeout;
  }

  // Returns true if the title text has been successfully set to the string
  // referred by |title_text_id|.
  bool SetTitleTextWithStringId(int title_text_id);

  // Returns true if the title text has been successfully set to the string
  // referred by |message_text_id|.
  bool SetMessageTextWithStringId(int message_text_id);

  void AppendButton(int button_id, const std::wstring& button_text);

  // Returns true if a button has been added successfully with the text set to
  // the string referred by |button_text_id|.
  bool AppendButtonWithStringId(int button_id, int button_text_id);

  // Shows the dialog and returns the ID of the button that the user clicked.
  // Returns nullopt if the dialog fails to show or times out.
  std::optional<int> Show();

  SimpleTaskDialog(const SimpleTaskDialog&) = delete;
  SimpleTaskDialog& operator=(const SimpleTaskDialog&) = delete;

 private:
  static HRESULT CALLBACK TaskDialogCallbackProc(HWND hwnd,
                                                 UINT notification,
                                                 WPARAM w_param,
                                                 LPARAM l_param,
                                                 LONG_PTR ref_data);

  SEQUENCE_CHECKER(sequence_checker_);

  HMODULE resource_module_;
  std::wstring title_text_;
  std::wstring message_text_;
  int default_button_ = -1;
  std::vector<std::pair<int, std::wstring>> dialog_buttons_;
  base::TimeDelta dialog_timeout_;

  // Tracks whether the dialog was in the foreground the last time we checked.
  // Default to true so we will attempt to bring it back if it starts in the
  // background for some reason.
  bool is_foreground_window_ GUARDED_BY_CONTEXT(sequence_checker_) = true;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_SIMPLE_TASK_DIALOG_H_
