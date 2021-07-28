// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_SET_UP_URL_FORWARDER_DIALOG_H_
#define REMOTING_HOST_WIN_SET_UP_URL_FORWARDER_DIALOG_H_

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "base/win/windows_types.h"

namespace remoting {

// Class that controls a dialog that prompts the user to change the default
// browser to the URL forwarder.
class SetUpUrlForwarderDialog final {
 public:
  SetUpUrlForwarderDialog();
  ~SetUpUrlForwarderDialog();

  void Show(base::OnceClosure on_continue, base::OnceClosure on_cancel);

  SetUpUrlForwarderDialog(const SetUpUrlForwarderDialog&) = delete;
  SetUpUrlForwarderDialog& operator=(const SetUpUrlForwarderDialog&) = delete;

 private:
  static INT_PTR CALLBACK DialogProc(HWND hwnd,
                                     UINT msg,
                                     WPARAM wParam,
                                     LPARAM lParam);

  INT_PTR OnDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  SEQUENCE_CHECKER(sequence_checker_);

  HWND hwnd_ = nullptr;

  base::OnceClosure on_continue_;
  base::OnceClosure on_cancel_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_SET_UP_URL_FORWARDER_DIALOG_H_
