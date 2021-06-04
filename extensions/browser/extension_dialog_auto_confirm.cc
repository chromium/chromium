// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "extensions/browser/extension_dialog_auto_confirm.h"

namespace extensions {

namespace {
ScopedTestDialogAutoConfirm::AutoConfirm g_extension_dialog_auto_confirm_value =
    ScopedTestDialogAutoConfirm::NONE;
int g_extension_dialog_option_to_select = 0;
}

ScopedTestDialogAutoConfirm::ScopedTestDialogAutoConfirm(
    ScopedTestDialogAutoConfirm::AutoConfirm override_confirm_value)
    : old_auto_confirm_value_(
          std::exchange(g_extension_dialog_auto_confirm_value,
                        override_confirm_value)),
      // Assign a default value if unspecified.
      old_option_to_select_(0) {}

ScopedTestDialogAutoConfirm::ScopedTestDialogAutoConfirm(
    ScopedTestDialogAutoConfirm::AutoConfirm override_confirm_value,
    int override_option_to_select)
    : old_auto_confirm_value_(
          std::exchange(g_extension_dialog_auto_confirm_value,
                        override_confirm_value)),
      old_option_to_select_(std::exchange(g_extension_dialog_option_to_select,
                                          override_option_to_select)) {}

ScopedTestDialogAutoConfirm::~ScopedTestDialogAutoConfirm() {
  g_extension_dialog_auto_confirm_value = old_auto_confirm_value_;
  g_extension_dialog_option_to_select = old_option_to_select_;
}

// static
ScopedTestDialogAutoConfirm::AutoConfirm
ScopedTestDialogAutoConfirm::GetAutoConfirmValue() {
  return g_extension_dialog_auto_confirm_value;
}

// static
int ScopedTestDialogAutoConfirm::GetOptionSelected() {
  return g_extension_dialog_option_to_select;
}

}  // namespace extensions
