// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_federated_permission_context.h"

namespace content {

ShellFederatedPermissionContext::ShellFederatedPermissionContext() = default;

ShellFederatedPermissionContext::~ShellFederatedPermissionContext() = default;

// FederatedIdentityRequestPermissionContextDelegate
bool ShellFederatedPermissionContext::HasRequestPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider) {
  return request_permissions_.find(std::make_pair(
             relying_party.Serialize(), identity_provider.Serialize())) !=
         request_permissions_.end();
}

void ShellFederatedPermissionContext::GrantRequestPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider) {
  request_permissions_.insert(
      std::make_pair(relying_party.Serialize(), identity_provider.Serialize()));
}

void ShellFederatedPermissionContext::RevokeRequestPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider) {
  request_permissions_.erase(
      std::make_pair(relying_party.Serialize(), identity_provider.Serialize()));
}

bool ShellFederatedPermissionContext::HasSharingPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  return sharing_permissions_.find(std::tuple(
             relying_party.Serialize(), identity_provider.Serialize(),
             account_id)) != sharing_permissions_.end();
}

void ShellFederatedPermissionContext::GrantSharingPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  sharing_permissions_.insert(std::tuple(
      relying_party.Serialize(), identity_provider.Serialize(), account_id));
}

void ShellFederatedPermissionContext::RevokeSharingPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  sharing_permissions_.erase(std::tuple(
      relying_party.Serialize(), identity_provider.Serialize(), account_id));
}

// FederatedIdentityActiveSessionPermissionContextDelegate
bool ShellFederatedPermissionContext::HasActiveSession(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  return active_sessions_.find(std::tuple(
             relying_party.Serialize(), identity_provider.Serialize(),
             account_identifier)) != active_sessions_.end();
}

void ShellFederatedPermissionContext::GrantActiveSession(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  active_sessions_.insert(std::tuple(relying_party.Serialize(),
                                     identity_provider.Serialize(),
                                     account_identifier));
}

void ShellFederatedPermissionContext::RevokeActiveSession(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  active_sessions_.erase(std::tuple(relying_party.Serialize(),
                                    identity_provider.Serialize(),
                                    account_identifier));
}

}  // namespace content
