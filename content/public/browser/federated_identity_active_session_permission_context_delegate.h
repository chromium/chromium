// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_ACTIVE_SESSION_PERMISSION_CONTEXT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_ACTIVE_SESSION_PERMISSION_CONTEXT_DELEGATE_H_

#include "url/origin.h"

namespace content {

// Delegate interface for the WebID implementation in content to query and
// manage permission grants associated with active federated account sessions
// between a Relying Party and Identity Provider origin. Each grant is
// associated with a specific account. Active sessions enable session
// management capabilities between the two origins.
class FederatedIdentityActiveSessionPermissionContextDelegate {
 public:
  FederatedIdentityActiveSessionPermissionContextDelegate() = default;
  virtual ~FederatedIdentityActiveSessionPermissionContextDelegate() = default;

  // Determine whether the `relying_party_requester` has an existing active
  // session for the specified `account_identifier` with the
  // `identity_provider`.
  virtual bool HasActiveSession(const url::Origin& relying_party_requester,
                                const url::Origin& identity_provider,
                                const std::string& account_identifier) = 0;

  // Grant active session capabilities between the `relying_party_requester` and
  // `identity_provider` origins for the specified account.
  virtual void GrantActiveSession(const url::Origin& relying_party_requester,
                                  const url::Origin& identity_provider,
                                  const std::string& account_identifier) = 0;

  // Revoke a previously-provided grant from the `relying_party_requester` to
  // the `identity_provider` for the specified account.
  virtual void RevokeActiveSession(const url::Origin& relying_party_requester,
                                   const url::Origin& identity_provider,
                                   const std::string& account_identifier) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_ACTIVE_SESSION_PERMISSION_CONTEXT_DELEGATE_H_
