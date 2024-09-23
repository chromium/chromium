// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_DIALOG_AUTO_CONFIRM_H_
#define EXTENSIONS_BROWSER_EXTENSION_DIALOG_AUTO_CONFIRM_H_

#include <string>

#include "base/auto_reset.h"

namespace extensions {

class ScopedTestDialogAutoConfirm {
 public:
  enum AutoConfirm {
    NONE,               // The prompt will show normally.
    ACCEPT,             // The prompt will always accept.
    ACCEPT_AND_OPTION,  // The prompt will always check an option (if
                        // any) and accept.
    CANCEL,             // The prompt will always cancel.
  };

  // Set up auto confirm value to |override_confirm_value| so the dialog is
  // automatically shown, accepted, or cancelled.
  explicit ScopedTestDialogAutoConfirm(AutoConfirm override_confirm_value);

  ScopedTestDialogAutoConfirm(const ScopedTestDialogAutoConfirm&) = delete;
  ScopedTestDialogAutoConfirm& operator=(const ScopedTestDialogAutoConfirm&) =
      delete;

  ~ScopedTestDialogAutoConfirm();

  // Return whether the dialog should be showed, accepted, or cancelled.
  static AutoConfirm GetAutoConfirmValue();

  // Return the stored string justification.
  static std::string GetJustification();

  // Store the provided string justification.
  void set_justification(const std::string& justification);

 private:
  // Preserve the old auto confirm value so it can be reset when the dialog
  // goes out of scope.
  const AutoConfirm old_auto_confirm_value_;

  // Preserve the old justification so it can be reset when the dialog goes out
  // of scope.
  std::string old_justification_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_DIALOG_AUTO_CONFIRM_H_
