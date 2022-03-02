// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_IDENTITY_DIALOG_CONTROLLER_H_
#define CONTENT_SHELL_BROWSER_SHELL_IDENTITY_DIALOG_CONTROLLER_H_

#include "content/public/browser/identity_request_dialog_controller.h"

namespace content {

class ShellIdentityDialogController : public IdentityRequestDialogController {
 public:
  void ShowAccountsDialog(content::WebContents* rp_web_contents,
                          const GURL& idp_signin_url,
                          base::span<const IdentityRequestAccount> accounts,
                          const IdentityProviderMetadata& idp_metadata,
                          const ClientIdData& client_id_data,
                          IdentityRequestAccount::SignInMode sign_in_mode,
                          AccountSelectionCallback on_selected) override;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_IDENTITY_DIALOG_CONTROLLER_H_
