// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SUPERVISED_USER_EXTENSIONS_DELEGATE_H_
#define EXTENSIONS_BROWSER_SUPERVISED_USER_EXTENSIONS_DELEGATE_H_

#include "base/callback.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

class SupervisedUserExtensionsDelegate {
 public:
  // Result of the parent permission dialog invocation.
  enum class ParentPermissionDialogResult {
    kParentPermissionReceived,
    kParentPermissionCanceled,
    kParentPermissionFailed,
  };

  using ParentPermissionDialogDoneCallback =
      base::OnceCallback<void(ParentPermissionDialogResult)>;

  virtual ~SupervisedUserExtensionsDelegate() = default;

  // Returns true if |context| represents a supervised child account.
  virtual bool IsChild(content::BrowserContext* context) const = 0;

  // Returns true if the parent has already approved the |extension|.
  virtual bool IsExtensionAllowedByParent(
      const extensions::Extension& extension,
      content::BrowserContext* context) const = 0;

  // If the current user is a child, the child user has a custodian/parent, and
  // the parent has enabled the "Permissions for sites, apps and extensions"
  // toggle, then display the Parent Permission Dialog and call
  // |parent_permission_callback|. Otherwise, display the Extension Install
  // Blocked by Parent Dialog and call |error_callback|. The two paths are
  // mutually exclusive.
  virtual void PromptForParentPermissionOrShowError(
      const extensions::Extension& extension,
      content::BrowserContext* browser_context,
      content::WebContents* web_contents,
      ParentPermissionDialogDoneCallback parent_permission_callback,
      base::OnceClosure error_callback) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SUPERVISED_USER_EXTENSIONS_DELEGATE_H_
