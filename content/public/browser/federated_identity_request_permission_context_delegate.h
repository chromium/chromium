// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_REQUEST_PERMISSION_CONTEXT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_REQUEST_PERMISSION_CONTEXT_DELEGATE_H_

#include "url/origin.h"

namespace content {

// Delegate interface for the WebID implementation in content to query and
// manage permission grants associated with the ability to send identity
// requests to a given provider.
class FederatedIdentityRequestPermissionContextDelegate {
 public:
  FederatedIdentityRequestPermissionContextDelegate() = default;
  virtual ~FederatedIdentityRequestPermissionContextDelegate() = default;

  // Determine whether the relying_party has an existing permission grant to
  // send identity requests to the Identity Provider.
  virtual bool HasRequestPermission(const url::Origin& relying_party,
                                    const url::Origin& identity_provider) = 0;

  // Grant permission for the relying_party to send requests to the given
  // provider.
  virtual void GrantRequestPermission(const url::Origin& relying_party,
                                      const url::Origin& identity_provider) = 0;

  // Revoke a previously-provided grant from the relying_party to the provider.
  virtual void RevokeRequestPermission(
      const url::Origin& relying_party,
      const url::Origin& identity_provider) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_REQUEST_PERMISSION_CONTEXT_DELEGATE_H_
