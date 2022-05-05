// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_federated_permission_context.h"

#include "base/feature_list.h"
#include "content/public/common/content_features.h"

namespace content {

ShellFederatedPermissionContext::ShellFederatedPermissionContext() = default;

ShellFederatedPermissionContext::~ShellFederatedPermissionContext() = default;

content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus
ShellFederatedPermissionContext::GetApiPermissionStatus(
    const url::Origin& rp_origin) {
  return base::FeatureList::IsEnabled(features::kFedCm)
             ? PermissionStatus::GRANTED
             : PermissionStatus::BLOCKED_VARIATIONS;
}

void ShellFederatedPermissionContext::RecordDismissAndEmbargo(
    const url::Origin& rp_origin) {}

void ShellFederatedPermissionContext::RemoveEmbargoAndResetCounts(
    const url::Origin& rp_origin) {}

bool ShellFederatedPermissionContext::HasSharingPermissionForAnyAccount(
    const url::Origin& relying_party,
    const url::Origin& identity_provider) {
  for (const std::tuple<std::string, std::string, std::string>& permission :
       sharing_permissions_) {
    if (std::get<0>(permission) == relying_party.Serialize() &&
        std::get<1>(permission) == identity_provider.Serialize())
      return true;
  }
  return false;
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
