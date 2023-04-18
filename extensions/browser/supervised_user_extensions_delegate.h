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
  virtual void RequestToAddExtensionOrShowError(
      const extensions::Extension& extension,
      content::WebContents* web_contents,
      const gfx::ImageSkia& icon,
      ExtensionApprovalDoneCallback extension_approval_callback) = 0;

  // Similar to RequestToAddExtensionOrShowError except for enabling already
  // installed extensions. The icon is fetched from local resources.
  virtual void RequestToEnableExtensionOrShowError(
      const extensions::Extension& extension,
      content::WebContents* web_contents,
      ExtensionApprovalDoneCallback extension_approval_callback) = 0;

  // Returns true if the primary account represents a supervised child account
  // who may install extensions with parent permission.
  virtual bool CanInstallExtensions() const = 0;

  // Updates the set of approved extensions to add approval for `extension`.
  virtual void AddExtensionApproval(const extensions::Extension& extension) = 0;

  // Updates the set of approved extensions to remove approval for `extension`.
  virtual void RemoveExtensionApproval(
      const extensions::Extension& extension) = 0;

  // Records when an extension has been enabled or disabled by parental
  // controls.
  virtual void RecordExtensionEnablementUmaMetrics(bool enabled) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SUPERVISED_USER_EXTENSIONS_DELEGATE_H_
