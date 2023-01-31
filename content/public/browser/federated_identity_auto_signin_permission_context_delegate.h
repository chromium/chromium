// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION_CONTEXT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION_CONTEXT_DELEGATE_H_

#include "content/common/content_export.h"

namespace content {

// Delegate interface for the FedCM implementation to query whether the FedCM
// API's auto sign-in is enabled in Site Settings.
class CONTENT_EXPORT FederatedIdentityAutoSigninPermissionContextDelegate {
 public:
  FederatedIdentityAutoSigninPermissionContextDelegate() = default;
  virtual ~FederatedIdentityAutoSigninPermissionContextDelegate() = default;

  // Returns the permission status of the FedCM API's auto sign-in feature.
  virtual bool HasAutoSigninPermission() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION_CONTEXT_DELEGATE_H_
