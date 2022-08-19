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

  // Determine whether there is an existing permission grant to share identity
  // information for the given account to the `relying_party_requester` when
  // embedded in `relying_party_embedder`.
  virtual bool HasSharingPermission(const url::Origin& relying_party_requester,
                                    const url::Origin& relying_party_embedder,
                                    const url::Origin& identity_provider,
                                    const std::string& account_id) = 0;

  // Grants permission to share identity information for the given account to
  // `relying_party_requester` when embedded in `relying_party_embedder`.
  virtual void GrantSharingPermission(
      const url::Origin& relying_party_requester,
      const url::Origin& relying_party_embedder,
      const url::Origin& identity_provider,
      const std::string& account_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_SHARING_PERMISSION_CONTEXT_DELEGATE_H_
