// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_SHARING_PERMISSION_CONTEXT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_SHARING_PERMISSION_CONTEXT_DELEGATE_H_

#include "url/origin.h"

namespace content {

// Delegate interface for the WebID implementation in content to query and
// manage permission grants associated with the ability to share identity
// information from a given provider to a given relying party.
class FederatedIdentitySharingPermissionContextDelegate {
 public:
  FederatedIdentitySharingPermissionContextDelegate() = default;
  virtual ~FederatedIdentitySharingPermissionContextDelegate() = default;

  // Determine whether the requester has an existing permission grant to share
  // identity information for the given account to the relying party.
  virtual bool HasSharingPermission(const url::Origin& relying_party,
                                    const url::Origin& identity_provider,
                                    const std::string& account_id) = 0;

  // Grant permission for the requester to share identity information for the
  // given account to the  relying party.
  virtual void GrantSharingPermission(const url::Origin& relying_party,
                                      const url::Origin& identity_provider,
                                      const std::string& account_id) = 0;

  // Revoke a previously-provided grant from the identity provider for the
  // relying party and the given account.
  virtual void RevokeSharingPermission(const url::Origin& relying_party,
                                       const url::Origin& identity_provider,
                                       const std::string& account_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_SHARING_PERMISSION_CONTEXT_DELEGATE_H_
