// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_federated_permission_context.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "content/public/common/content_features.h"
#include "content/shell/common/shell_switches.h"

namespace content {

ShellFederatedPermissionContext::ShellFederatedPermissionContext() = default;

ShellFederatedPermissionContext::~ShellFederatedPermissionContext() = default;

content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus
ShellFederatedPermissionContext::GetApiPermissionStatus(
    const url::Origin& relying_party_embedder) {
  if (!base::FeatureList::IsEnabled(features::kFedCm)) {
    return PermissionStatus::BLOCKED_VARIATIONS;
  }

  if (embargoed_origins_.count(relying_party_embedder)) {
    return PermissionStatus::BLOCKED_EMBARGO;
  }

  return PermissionStatus::GRANTED;
}

// FederatedIdentityApiPermissionContextDelegate
void ShellFederatedPermissionContext::RecordDismissAndEmbargo(
    const url::Origin& relying_party_embedder) {
  embargoed_origins_.insert(relying_party_embedder);
}

void ShellFederatedPermissionContext::RemoveEmbargoAndResetCounts(
    const url::Origin& relying_party_embedder) {
  embargoed_origins_.erase(relying_party_embedder);
}

bool ShellFederatedPermissionContext::ShouldCompleteRequestImmediately() const {
  return switches::IsRunWebTestsSwitchPresent();
}

bool ShellFederatedPermissionContext::HasThirdPartyCookiesAccess(
    content::RenderFrameHost& host,
    const GURL& provider_url,
    const url::Origin& relying_party_embedder) const {
  return std::find_if(
             has_third_party_cookies_access_.begin(),
             has_third_party_cookies_access_.end(), [&](const auto& entry) {
               return provider_url.spec() == std::get<0>(entry) &&
                      relying_party_embedder.Serialize() == std::get<1>(entry);
             }) != has_third_party_cookies_access_.end();
}

void ShellFederatedPermissionContext::SetHasThirdPartyCookiesAccessForTesting(
    const std::string& identity_provider,
    const std::string& relying_party_embedder) {
  has_third_party_cookies_access_.insert(
      std::pair(identity_provider, relying_party_embedder));
}

// FederatedIdentityAutoReauthnPermissionContextDelegate
bool ShellFederatedPermissionContext::IsAutoReauthnSettingEnabled() {
  return auto_reauthn_permission_;
}

bool ShellFederatedPermissionContext::IsAutoReauthnEmbargoed(
    const url::Origin& relying_party_embedder) {
  return false;
}

void ShellFederatedPermissionContext::SetRequiresUserMediation(
    const GURL& rp_url,
    bool requires_user_mediation) {
  if (requires_user_mediation) {
    require_user_mediation_sites_.insert(rp_url);
  } else {
    require_user_mediation_sites_.erase(rp_url);
  }
}

bool ShellFederatedPermissionContext::RequiresUserMediation(
    const GURL& rp_url) {
  return require_user_mediation_sites_.contains(rp_url);
}

base::Time ShellFederatedPermissionContext::GetAutoReauthnEmbargoStartTime(
    const url::Origin& relying_party_embedder) {
  return base::Time();
}

void ShellFederatedPermissionContext::RecordEmbargoForAutoReauthn(
    const url::Origin& relying_party_embedder) {}

void ShellFederatedPermissionContext::RemoveEmbargoForAutoReauthn(
    const url::Origin& relying_party_embedder) {}

void ShellFederatedPermissionContext::AddIdpSigninStatusObserver(
    IdpSigninStatusObserver* observer) {
  idp_signin_status_observer_list_.AddObserver(observer);
}

void ShellFederatedPermissionContext::RemoveIdpSigninStatusObserver(
    IdpSigninStatusObserver* observer) {
  idp_signin_status_observer_list_.RemoveObserver(observer);
}

bool ShellFederatedPermissionContext::HasSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const absl::optional<std::string>& account_id) {
  bool skip_account_check = !account_id;
  return std::find_if(sharing_permissions_.begin(), sharing_permissions_.end(),
                      [&](const auto& entry) {
                        return relying_party_requester.Serialize() ==
                                   std::get<0>(entry) &&
                               relying_party_embedder.Serialize() ==
                                   std::get<1>(entry) &&
                               identity_provider.Serialize() ==
                                   std::get<2>(entry) &&
                               (skip_account_check ||
                                account_id.value() == std::get<3>(entry));
                      }) != sharing_permissions_.end();
}

bool ShellFederatedPermissionContext::HasSharingPermission(
    const url::Origin& relying_party_requester) {
  return std::find_if(sharing_permissions_.begin(), sharing_permissions_.end(),
                      [&](const auto& entry) {
                        return relying_party_requester.Serialize() ==
                               std::get<0>(entry);
                      }) != sharing_permissions_.end();
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

void ShellFederatedPermissionContext::RevokeSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  size_t removed = sharing_permissions_.erase(std::tuple(
      relying_party_requester.Serialize(), relying_party_embedder.Serialize(),
      identity_provider.Serialize(), account_id));
  // If we did not remove any sharing permission, to preserve strong privacy
  // guarantees of the FedCM API, remove all sharing permissions associated with
  // the (`relying_party_requester`, `relying_party_embedder`,
  // `identity_provider` triple). This disabled auto re-authentication on that
  // account and means revocation may not be invoked repeatedly after a single
  // successful FedCM flow.
  if (!removed && !sharing_permissions_.empty()) {
    auto it = sharing_permissions_.begin();
    while (it != sharing_permissions_.end()) {
      const auto& [requester, embedder, idp, account] = *it;
      if (requester == relying_party_requester.Serialize() &&
          embedder == relying_party_embedder.Serialize() &&
          idp == identity_provider.Serialize()) {
        it = sharing_permissions_.erase(it);
      } else {
        ++it;
      }
    }
  }
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
  for (IdpSigninStatusObserver& observer : idp_signin_status_observer_list_) {
    observer.OnIdpSigninStatusReceived(idp_origin, idp_signin_status);
  }

  // TODO(crbug.com/1382989): Replace this with AddIdpSigninStatusObserver.
  if (idp_signin_status_closure_)
    idp_signin_status_closure_.Run();
}

void ShellFederatedPermissionContext::RegisterIdP(const ::GURL& configURL) {
  idp_registry_.push_back(configURL);
}

void ShellFederatedPermissionContext::UnregisterIdP(const ::GURL& configURL) {
  idp_registry_.erase(
      std::remove(idp_registry_.begin(), idp_registry_.end(), configURL),
      idp_registry_.end());
}

std::vector<GURL> ShellFederatedPermissionContext::GetRegisteredIdPs() {
  return idp_registry_;
}

}  // namespace content
