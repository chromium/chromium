// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SUPERVISED_USER_EXTENSIONS_DELEGATE_H_
#define EXTENSIONS_BROWSER_SUPERVISED_USER_EXTENSIONS_DELEGATE_H_

#include "base/functional/callback.h"
#include "extensions/common/extension.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

// These enum values represent the supervised user flows that lead to
// displaying the Extensions parent approval dialog.
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SupervisedUserExtensionParentApprovalEntryPoint)
enum class SupervisedUserExtensionParentApprovalEntryPoint : int {
  // Recorded when the dialog appears as part of installing a new extension
  // from Webstore.
  kOnWebstoreInstallation = 0,
  // Recorded when the dialog appears on enabling an existing extension which
  // is missing parent approval from the extension management page.
  kOnExtensionManagementSetEnabledOperation = 1,
  // Recorded the dialog appears on enabling an existing disabled/terminated
  // extension which is missing parent approval through the extension enable
  // flow.
  kOnTerminatedExtensionEnableFlowOperation = 2,
  // Add future entries above this comment, in sync with
  // "SupervisedUserExtensionParentApprovalEntryPoint" in
  // src/tools/metrics/histograms/metadata/families/enums.xml.
  // Update kMaxValue to the last value.
  kMaxValue = kOnTerminatedExtensionEnableFlowOperation
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:SupervisedUserExtensionParentApprovalEntryPoint)

namespace extensions {

class SupervisedUserExtensionsDelegate {
 public:
  // Result of the extension approval flow.
  enum class ExtensionApprovalResult {
    kApproved,  // Extension installation was approved.
    kCanceled,  // Extension approval flow was canceled.
    kFailed,    // Extension approval failed due to an error.
    kBlocked,   // Extension installation has been blocked by a parent.
  };

  using ExtensionApprovalDoneCallback =
      base::OnceCallback<void(ExtensionApprovalResult)>;

  virtual ~SupervisedUserExtensionsDelegate() = default;

  // Updates registration of management policy provider for supervised users.
  virtual void UpdateManagementPolicyRegistration() = 0;

  // Returns true if the primary account is a supervised child.
  virtual bool IsChild() const = 0;

  // Returns true if the parent has already approved the `extension`.
  virtual bool IsExtensionAllowedByParent(
      const extensions::Extension& extension) const = 0;

  // If the current user is a child, the child user has a custodian/parent, and
  // the parent has enabled the "Permissions for sites, apps and extensions"
  // toggle, then display the Parent Permission Dialog. If the setting is
  // disabled, the extension install blocked dialog is shown. When the flow is
  // complete call |extension_approval_callback|.
  // The icon must be supplied for installing new extensions because they are
  // fetched via a network request.
  // The extension approval dialog entry point indicates who invokes this method
  // and is persistent in metrics.
  virtual void RequestToAddExtensionOrShowError(
      const extensions::Extension& extension,
      content::WebContents* web_contents,
      const gfx::ImageSkia& icon,
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point,
      ExtensionApprovalDoneCallback extension_approval_callback) = 0;

  // Similar to RequestToAddExtensionOrShowError except for enabling already
  // installed extensions. The icon is fetched from local resources.
  virtual void RequestToEnableExtensionOrShowError(
      const extensions::Extension& extension,
      content::WebContents* web_contents,
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point,
      ExtensionApprovalDoneCallback extension_approval_callback) = 0;

  // Returns true if the primary account represents a supervised child account
  // who may install extensions with parent permission.
  virtual bool CanInstallExtensions() const = 0;

  // Updates the set of approved extensions to add approval for `extension`.
  virtual void AddExtensionApproval(const extensions::Extension& extension) = 0;

  // Checks if the given `extension` escalated permissions and records the
  // corresponding metrics.
  virtual void MaybeRecordPermissionsIncreaseMetrics(
      const extensions::Extension& extension) = 0;

  // Updates the set of approved extensions to remove approval for `extension`.
  virtual void RemoveExtensionApproval(
      const extensions::Extension& extension) = 0;

  // Records when an extension has been enabled or disabled by parental
  // controls.
  virtual void RecordExtensionEnablementUmaMetrics(bool enabled) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SUPERVISED_USER_EXTENSIONS_DELEGATE_H_
