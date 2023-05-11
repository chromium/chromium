// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_MODAL_DIALOG_VIEW_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_MODAL_DIALOG_VIEW_DELEGATE_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

// Delegate to control FedCM's modal dialogs. An example of a use case is if a
// user is signed-in according to the FedCM IDP Sign-in Status API but we find
// that the user has no accounts, a failure dialog is shown. The failure dialog
// contains a button which opens a modal dialog and allows the user to complete
// the sign-in flow on the modal dialog. This delegate controls that modal
// dialog.
class CONTENT_EXPORT FederatedIdentityModalDialogViewDelegate {
 public:
  FederatedIdentityModalDialogViewDelegate() = default;
  virtual ~FederatedIdentityModalDialogViewDelegate() = default;

  // Closes the FedCM modal dialog, if any.
  virtual void NotifyClose() = 0;

  // The modal dialog has provided a token to resolve the original request.
  // TODO(crbug.com/1429083): pass the configURL in the notification.
  virtual bool NotifyResolve(const std::string& token) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_MODAL_DIALOG_VIEW_DELEGATE_H_
