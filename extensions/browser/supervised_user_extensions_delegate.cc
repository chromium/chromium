// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/supervised_user_extensions_delegate.h"

#include "base/notimplemented.h"

namespace extensions {

void SupervisedUserExtensionsDelegate::UpdateManagementPolicyRegistration() {
  NOTIMPLEMENTED();
}

bool SupervisedUserExtensionsDelegate::IsChild() const {
  NOTIMPLEMENTED();
  return false;
}

bool SupervisedUserExtensionsDelegate::IsExtensionAllowedByParent(
    const extensions::Extension& extension) const {
  NOTIMPLEMENTED();
  return false;
}

void SupervisedUserExtensionsDelegate::RequestToAddExtensionOrShowError(
    const extensions::Extension& extension,
    content::WebContents* web_contents,
    const gfx::ImageSkia& icon,
    ExtensionApprovalDoneCallback extension_approval_callback) {
  NOTIMPLEMENTED();
  std::move(extension_approval_callback)
      .Run(SupervisedExtensionApprovalResult::kBlocked);
}

void SupervisedUserExtensionsDelegate::RequestToEnableExtensionOrShowError(
    const extensions::Extension& extension,
    content::WebContents* web_contents,
    ExtensionApprovalDoneCallback extension_approval_callback) {
  NOTIMPLEMENTED();
  std::move(extension_approval_callback)
      .Run(SupervisedExtensionApprovalResult::kBlocked);
}

bool SupervisedUserExtensionsDelegate::CanInstallExtensions() const {
  NOTIMPLEMENTED();
  return false;
}

void SupervisedUserExtensionsDelegate::AddExtensionApproval(
    const extensions::Extension& extension) {
  NOTIMPLEMENTED();
}

void SupervisedUserExtensionsDelegate::MaybeRecordPermissionsIncreaseMetrics(
    const extensions::Extension& extension) {
  NOTIMPLEMENTED();
}

void SupervisedUserExtensionsDelegate::RemoveExtensionApproval(
    const extensions::Extension& extension) {
  NOTIMPLEMENTED();
}

void SupervisedUserExtensionsDelegate::RecordExtensionEnablementUmaMetrics(
    bool enabled) const {
  NOTIMPLEMENTED();
}

}  // namespace extensions
