// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include "base/callback.h"
#include "content/common/content_export.h"

class GURL;

namespace content {
class WebContents;

// IdentityRequestDialogController is in interface for control of the UI
// surfaces that are displayed to intermediate the exchange of ID tokens.
class CONTENT_EXPORT IdentityRequestDialogController {
 public:
  enum class UserApproval {
    kApproved,
    kDenied,
  };

  using InitialApprovalCallback = base::OnceCallback<void(UserApproval)>;
  using IdProviderWindowClosedCallback = base::OnceCallback<void()>;
  using TokenExchangeApprovalCallback = base::OnceCallback<void(UserApproval)>;

  IdentityRequestDialogController() = default;

  IdentityRequestDialogController(const IdentityRequestDialogController&) =
      delete;
  IdentityRequestDialogController& operator=(
      const IdentityRequestDialogController&) = delete;

  virtual ~IdentityRequestDialogController() = default;

  // Permission-oriented flow methods.
  virtual void ShowInitialPermissionDialog(WebContents*,
                                           InitialApprovalCallback) = 0;
  virtual void ShowIdProviderWindow(WebContents*,
                                    const GURL& idp_signin_url,
                                    IdProviderWindowClosedCallback) = 0;
  virtual void ShowTokenExchangePermissionDialog(
      TokenExchangeApprovalCallback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
