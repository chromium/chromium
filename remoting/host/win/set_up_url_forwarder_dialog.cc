// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/set_up_url_forwarder_dialog.h"

#include <windows.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/win/current_module.h"
#include "remoting/host/win/core_resource.h"

namespace remoting {

SetUpUrlForwarderDialog::SetUpUrlForwarderDialog() = default;

SetUpUrlForwarderDialog::~SetUpUrlForwarderDialog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (hwnd_) {
    ::DestroyWindow(hwnd_);
  }
}

void SetUpUrlForwarderDialog::Show(base::OnceClosure on_continue,
                                   base::OnceClosure on_cancel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!on_continue_);
  DCHECK(!on_cancel_);
  DCHECK(!hwnd_);

  on_continue_ = std::move(on_continue);
  on_cancel_ = std::move(on_cancel);
  hwnd_ = CreateDialogParam(CURRENT_MODULE(),
                            MAKEINTRESOURCE(IDD_SET_UP_URL_FORWARDER), nullptr,
                            DialogProc, (LPARAM)this);
  if (!hwnd_) {
    LOG(ERROR) << "Unable to create dialog for setting up the URL forwarder";
    return;
  }
  ShowWindow(hwnd_, SW_SHOW);
}

// static
INT_PTR CALLBACK SetUpUrlForwarderDialog::DialogProc(HWND hwnd,
                                                     UINT msg,
                                                     WPARAM wParam,
                                                     LPARAM lParam) {
  SetUpUrlForwarderDialog* self = nullptr;
  if (msg == WM_INITDIALOG) {
    self = reinterpret_cast<SetUpUrlForwarderDialog*>(lParam);
    CHECK(self);
    SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)self);
  } else {
    LONG_PTR lp = GetWindowLongPtr(hwnd, DWLP_USER);
    self = reinterpret_cast<SetUpUrlForwarderDialog*>(lp);
  }
  if (self == nullptr) {
    return FALSE;
  }
  return self->OnDialogMessage(hwnd, msg, wParam, lParam);
}

INT_PTR SetUpUrlForwarderDialog::OnDialogMessage(HWND hwnd,
                                                 UINT msg,
                                                 WPARAM wParam,
                                                 LPARAM lParam) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (msg) {
    case WM_CLOSE:
      // Ignore close messages.
      return TRUE;
    case WM_DESTROY:
      // Ensure we don't try to use the HWND anymore.
      hwnd_ = nullptr;
      return TRUE;
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDC_SET_UP_URL_FORWARDER_DEFAULT:
          std::move(on_continue_).Run();
          ::EndDialog(hwnd, LOWORD(wParam));
          hwnd_ = nullptr;
          return TRUE;
        case IDC_SET_UP_URL_FORWARDER_CANCEL:
          std::move(on_cancel_).Run();
          ::EndDialog(hwnd, LOWORD(wParam));
          hwnd_ = nullptr;
          return TRUE;
      }
  }
  return FALSE;
}

}  // namespace remoting
