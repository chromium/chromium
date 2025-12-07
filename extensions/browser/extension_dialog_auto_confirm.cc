// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_dialog_auto_confirm.h"

#include <string>
#include <utility>

#include "base/lazy_instance.h"

namespace extensions {

namespace {

ScopedTestDialogAutoConfirm::AutoConfirm g_extension_dialog_auto_confirm_value =
    ScopedTestDialogAutoConfirm::NONE;

base::LazyInstance<std::string>::DestructorAtExit
    g_extension_dialog_justification = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ScopedTestDialogAutoConfirm::ScopedTestDialogAutoConfirm(
    ScopedTestDialogAutoConfirm::AutoConfirm override_confirm_value)
    : old_auto_confirm_value_(
          std::exchange(g_extension_dialog_auto_confirm_value,
                        override_confirm_value)) {}

ScopedTestDialogAutoConfirm::~ScopedTestDialogAutoConfirm() {
  g_extension_dialog_auto_confirm_value = old_auto_confirm_value_;
  g_extension_dialog_justification.Get() = old_justification_.c_str();
}

// static
ScopedTestDialogAutoConfirm::AutoConfirm
ScopedTestDialogAutoConfirm::GetAutoConfirmValue() {
  return g_extension_dialog_auto_confirm_value;
}

// static
std::string ScopedTestDialogAutoConfirm::GetJustification() {
  return g_extension_dialog_justification.Get();
}

void ScopedTestDialogAutoConfirm::set_justification(
    const std::string& justification) {
  old_justification_.assign(g_extension_dialog_justification.Get());
  g_extension_dialog_justification.Get() = justification;
}

}  // namespace extensions
