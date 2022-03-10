// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_DELEGATE_H_

namespace content {

// Delegate interface for the FedCM implementation to query whether the FedCM
// API is enabled in Site Settings.
class FederatedIdentityApiPermissionContextDelegate {
 public:
  FederatedIdentityApiPermissionContextDelegate() = default;
  virtual ~FederatedIdentityApiPermissionContextDelegate() = default;

  // Returns whether the FedCM API is enabled in site settings.
  virtual bool HasApiPermission() = 0;

  // Returns whether third party cookies are blocked.
  virtual bool AreThirdPartyCookiesBlocked() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_DELEGATE_H_
