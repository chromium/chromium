// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SUPERVISED_USER_EXTENSIONS_DELEGATE_H_
#define EXTENSIONS_BROWSER_SUPERVISED_USER_EXTENSIONS_DELEGATE_H_

#include "base/functional/callback.h"
#include "extensions/browser/supervised_extension_approval_result.h"
#include "extensions/common/extension.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace extensions {

// Interface for the supervised user extensions delegate. The interface has
// stub implementations so it can be used in test code.
class SupervisedUserExtensionsDelegate {
 public:
  using ExtensionApprovalDoneCallback =
      base::OnceCallback<void(SupervisedExtensionApprovalResult)>;

  SupervisedUserExtensionsDelegate() = default;
  virtual ~SupervisedUserExtensionsDelegate() = default;

  // Updates registration of management policy provider for supervised users.
  virtual void UpdateManagementPolicyRegistration();

  // Returns true if the primary account is a supervised child.
  virtual bool IsChild() const;

  // Returns true if the parent has already approved the `extension`.
  virtual bool IsExtensionAllowedByParent(
      const extensions::Extension& extension) const;

  // If the current user is a child, the child user has a custodian/parent, and
  // the parent has enabled the "Permissions for sites, apps and extensions"
  // toggle, then display the Parent Permission Dialog. If the setting is
  // disabled, the extension install blocked dialog is shown. When the flow is
  // complete call `extension_approval_callback`.
  // The icon must be supplied for installing new extensions because they are
  // fetched via a network request.
  // The extension approval dialog entry point indicates who invokes this method
  // and is persistent in metrics.
  virtual void RequestToAddExtensionOrShowError(
      const extensions::Extension& extension,
      content::WebContents* web_contents,
      const gfx::ImageSkia& icon,
      ExtensionApprovalDoneCallback extension_approval_callback);

  // Similar to RequestToAddExtensionOrShowError except for enabling already
  // installed extensions. The icon is fetched from local resources.
  virtual void RequestToEnableExtensionOrShowError(
      const extensions::Extension& extension,
      content::WebContents* web_contents,
      ExtensionApprovalDoneCallback extension_approval_callback);

  // Returns true if the primary account represents a supervised child account
  // who may install extensions with parent permission.
  virtual bool CanInstallExtensions() const;

  // Updates the set of approved extensions to add approval for `extension`.
  virtual void AddExtensionApproval(const extensions::Extension& extension);

  // Checks if the given `extension` escalated permissions and records the
  // corresponding metrics.
  virtual void MaybeRecordPermissionsIncreaseMetrics(
      const extensions::Extension& extension);

  // Updates the set of approved extensions to remove approval for `extension`.
  virtual void RemoveExtensionApproval(const extensions::Extension& extension);

  // Records when an extension has been enabled or disabled by parental
  // controls.
  virtual void RecordExtensionEnablementUmaMetrics(bool enabled) const;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SUPERVISED_USER_EXTENSIONS_DELEGATE_H_
