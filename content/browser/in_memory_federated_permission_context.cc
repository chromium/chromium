// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_memory_federated_permission_context.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "content/public/common/content_features.h"

namespace content {

InMemoryFederatedPermissionContext::InMemoryFederatedPermissionContext() =
    default;

InMemoryFederatedPermissionContext::~InMemoryFederatedPermissionContext() =
    default;

content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus
InMemoryFederatedPermissionContext::GetApiPermissionStatus(
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
void InMemoryFederatedPermissionContext::RecordDismissAndEmbargo(
    const url::Origin& relying_party_embedder) {
  embargoed_origins_.insert(relying_party_embedder);
}

void InMemoryFederatedPermissionContext::RemoveEmbargoAndResetCounts(
    const url::Origin& relying_party_embedder) {
  embargoed_origins_.erase(relying_party_embedder);
}

bool InMemoryFederatedPermissionContext::ShouldCompleteRequestImmediately()
    const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch("run-web-tests");
}

bool InMemoryFederatedPermissionContext::HasThirdPartyCookiesAccess(
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

void InMemoryFederatedPermissionContext::
    SetHasThirdPartyCookiesAccessForTesting(
        const std::string& identity_provider,
        const std::string& relying_party_embedder) {
  has_third_party_cookies_access_.insert(
      std::pair(identity_provider, relying_party_embedder));
}

// FederatedIdentityAutoReauthnPermissionContextDelegate
bool InMemoryFederatedPermissionContext::IsAutoReauthnSettingEnabled() {
  return auto_reauthn_permission_;
}

bool InMemoryFederatedPermissionContext::IsAutoReauthnEmbargoed(
    const url::Origin& relying_party_embedder) {
  return false;
}

void InMemoryFederatedPermissionContext::SetRequiresUserMediation(
    const url::Origin& rp_origin,
    bool requires_user_mediation) {
  if (requires_user_mediation) {
    require_user_mediation_sites_.insert(net::SchemefulSite(rp_origin));
  } else {
    require_user_mediation_sites_.erase(net::SchemefulSite(rp_origin));
  }
  OnSetRequiresUserMediation(rp_origin, base::DoNothing());
}

bool InMemoryFederatedPermissionContext::RequiresUserMediation(
    const url::Origin& rp_origin) {
  return require_user_mediation_sites_.contains(net::SchemefulSite(rp_origin));
}

void InMemoryFederatedPermissionContext::OnSetRequiresUserMediation(
    const url::Origin& relying_party,
    base::OnceClosure callback) {
  std::move(callback).Run();
}

base::Time InMemoryFederatedPermissionContext::GetAutoReauthnEmbargoStartTime(
    const url::Origin& relying_party_embedder) {
  return base::Time();
}

void InMemoryFederatedPermissionContext::RecordEmbargoForAutoReauthn(
    const url::Origin& relying_party_embedder) {}

void InMemoryFederatedPermissionContext::RemoveEmbargoForAutoReauthn(
    const url::Origin& relying_party_embedder) {}

void InMemoryFederatedPermissionContext::AddIdpSigninStatusObserver(
    IdpSigninStatusObserver* observer) {
  if (idp_signin_status_observer_list_.HasObserver(observer)) {
    return;
  }

  idp_signin_status_observer_list_.AddObserver(observer);
}

void InMemoryFederatedPermissionContext::RemoveIdpSigninStatusObserver(
    IdpSigninStatusObserver* observer) {
  idp_signin_status_observer_list_.RemoveObserver(observer);
}

bool InMemoryFederatedPermissionContext::HasSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider) {
  return std::find_if(sharing_permissions_.begin(), sharing_permissions_.end(),
                      [&](const auto& entry) {
                        return relying_party_requester.Serialize() ==
                                   std::get<0>(entry) &&
                               relying_party_embedder.Serialize() ==
                                   std::get<1>(entry) &&
                               identity_provider.Serialize() ==
                                   std::get<2>(entry);
                      }) != sharing_permissions_.end();
}

std::optional<base::Time>
InMemoryFederatedPermissionContext::GetLastUsedTimestamp(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  return std::find_if(sharing_permissions_.begin(), sharing_permissions_.end(),
                      [&](const auto& entry) {
                        return relying_party_requester.Serialize() ==
                                   std::get<0>(entry) &&
                               relying_party_embedder.Serialize() ==
                                   std::get<1>(entry) &&
                               identity_provider.Serialize() ==
                                   std::get<2>(entry) &&
                               account_id == std::get<3>(entry);
                      }) != sharing_permissions_.end()
             ? std::make_optional<base::Time>()
             : std::nullopt;
}

bool InMemoryFederatedPermissionContext::HasSharingPermission(
    const url::Origin& relying_party_requester) {
  return std::find_if(sharing_permissions_.begin(), sharing_permissions_.end(),
                      [&](const auto& entry) {
                        return relying_party_requester.Serialize() ==
                               std::get<0>(entry);
                      }) != sharing_permissions_.end();
}

void InMemoryFederatedPermissionContext::GrantSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  sharing_permissions_.insert(std::tuple(
      relying_party_requester.Serialize(), relying_party_embedder.Serialize(),
      identity_provider.Serialize(), account_id));
}

void InMemoryFederatedPermissionContext::RevokeSharingPermission(
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

void InMemoryFederatedPermissionContext::RefreshExistingSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  // `sharing_permissions_` does not currently store timestamps, so this method
  // does nothing.
}

std::optional<bool> InMemoryFederatedPermissionContext::GetIdpSigninStatus(
    const url::Origin& idp_origin) {
  auto idp_signin_status = idp_signin_status_.find(idp_origin.Serialize());
  if (idp_signin_status != idp_signin_status_.end()) {
    return idp_signin_status->second;
  } else {
    return std::nullopt;
  }
}

void InMemoryFederatedPermissionContext::SetIdpSigninStatus(
    const url::Origin& idp_origin,
    bool idp_signin_status) {
  idp_signin_status_[idp_origin.Serialize()] = idp_signin_status;
  for (IdpSigninStatusObserver& observer : idp_signin_status_observer_list_) {
    observer.OnIdpSigninStatusReceived(idp_origin, idp_signin_status);
  }

  // TODO(crbug.com/40245925): Replace this with AddIdpSigninStatusObserver.
  if (idp_signin_status_closure_) {
    idp_signin_status_closure_.Run();
  }
}

void InMemoryFederatedPermissionContext::RegisterIdP(const ::GURL& configURL) {
  idp_registry_.push_back(configURL);
}

void InMemoryFederatedPermissionContext::UnregisterIdP(
    const ::GURL& configURL) {
  idp_registry_.erase(
      std::remove(idp_registry_.begin(), idp_registry_.end(), configURL),
      idp_registry_.end());
}

std::vector<GURL> InMemoryFederatedPermissionContext::GetRegisteredIdPs() {
  return idp_registry_;
}

void InMemoryFederatedPermissionContext::ResetForTesting() {
  request_permissions_.clear();
  sharing_permissions_.clear();
  idp_signin_status_.clear();
  has_third_party_cookies_access_.clear();
  idp_signin_status_observer_list_.Clear();
  idp_signin_status_closure_.Reset();
  auto_reauthn_permission_ = true;
  idp_registry_.clear();
  embargoed_origins_.clear();
  require_user_mediation_sites_.clear();
}

}  // namespace content
