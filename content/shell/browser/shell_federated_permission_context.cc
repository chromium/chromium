// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_federated_permission_context.h"

#include "base/feature_list.h"
#include "content/public/common/content_features.h"
#include "content/shell/common/shell_switches.h"

namespace content {

ShellFederatedPermissionContext::ShellFederatedPermissionContext() = default;

ShellFederatedPermissionContext::~ShellFederatedPermissionContext() = default;

content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus
ShellFederatedPermissionContext::GetApiPermissionStatus(
    const url::Origin& relying_party_embedder) {
  return base::FeatureList::IsEnabled(features::kFedCm)
             ? PermissionStatus::GRANTED
             : PermissionStatus::BLOCKED_VARIATIONS;
}

void ShellFederatedPermissionContext::RecordDismissAndEmbargo(
    const url::Origin& relying_party_embedder) {}

void ShellFederatedPermissionContext::RemoveEmbargoAndResetCounts(
    const url::Origin& relying_party_embedder) {}

bool ShellFederatedPermissionContext::ShouldCompleteRequestImmediately() const {
  return switches::IsRunWebTestsSwitchPresent();
}

// FederatedIdentityActiveSessionPermissionContextDelegate
bool ShellFederatedPermissionContext::HasActiveSession(
    const url::Origin& relying_party_requester,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  return active_sessions_.find(std::tuple(
             relying_party_requester.Serialize(), identity_provider.Serialize(),
             account_identifier)) != active_sessions_.end();
}

void ShellFederatedPermissionContext::GrantActiveSession(
    const url::Origin& relying_party_requester,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  active_sessions_.insert(std::tuple(relying_party_requester.Serialize(),
                                     identity_provider.Serialize(),
                                     account_identifier));
}

void ShellFederatedPermissionContext::RevokeActiveSession(
    const url::Origin& relying_party_requester,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  active_sessions_.erase(std::tuple(relying_party_requester.Serialize(),
                                    identity_provider.Serialize(),
                                    account_identifier));
}

bool ShellFederatedPermissionContext::HasSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  return sharing_permissions_.find(std::tuple(
             relying_party_requester.Serialize(),
             relying_party_embedder.Serialize(), identity_provider.Serialize(),
             account_id)) != sharing_permissions_.end();
}

void ShellFederatedPermissionContext::GrantSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  sharing_permissions_.insert(std::tuple(
      relying_party_requester.Serialize(), relying_party_embedder.Serialize(),
      identity_provider.Serialize(), account_id));
}

absl::optional<bool> ShellFederatedPermissionContext::GetIdpSigninStatus(
    const url::Origin& idp_origin) {
  auto idp_signin_status = idp_signin_status_.find(idp_origin.Serialize());
  if (idp_signin_status != idp_signin_status_.end()) {
    return idp_signin_status->second;
  } else {
    return absl::nullopt;
  }
}

void ShellFederatedPermissionContext::SetIdpSigninStatus(
    const url::Origin& idp_origin,
    bool idp_signin_status) {
  idp_signin_status_[idp_origin.Serialize()] = idp_signin_status;
  // TODO(crbug.com/1382989): Find a better way to do this than adding
  // explicit helper code to signal completion.
  if (idp_signin_status_closure_)
    idp_signin_status_closure_.Run();
}

}  // namespace content
