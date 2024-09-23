// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_MEMORY_FEDERATED_PERMISSION_CONTEXT_H_
#define CONTENT_BROWSER_IN_MEMORY_FEDERATED_PERMISSION_CONTEXT_H_

#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_auto_reauthn_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"

namespace url {
class Origin;
}

namespace content {

// This class implements the various FedCM delegates. It is used to store
// permission and login state in memory as a default implementation.
class InMemoryFederatedPermissionContext
    : public FederatedIdentityApiPermissionContextDelegate,
      public FederatedIdentityAutoReauthnPermissionContextDelegate,
      public FederatedIdentityPermissionContextDelegate {
 public:
  InMemoryFederatedPermissionContext();
  ~InMemoryFederatedPermissionContext() override;

  // FederatedIdentityApiPermissionContextDelegate
  content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus
  GetApiPermissionStatus(const url::Origin& relying_party_embedder) override;
  void RecordDismissAndEmbargo(
      const url::Origin& relying_party_embedder) override;
  void RemoveEmbargoAndResetCounts(
      const url::Origin& relying_party_embedder) override;
  bool ShouldCompleteRequestImmediately() const override;
  bool HasThirdPartyCookiesAccess(
      content::RenderFrameHost& host,
      const GURL& provider_url,
      const url::Origin& relying_party_embedder) const override;

  // FederatedIdentityAutoReauthnPermissionContextDelegate
  bool IsAutoReauthnSettingEnabled() override;
  bool IsAutoReauthnEmbargoed(
      const url::Origin& relying_party_embedder) override;
  base::Time GetAutoReauthnEmbargoStartTime(
      const url::Origin& relying_party_embedder) override;
  void RecordEmbargoForAutoReauthn(
      const url::Origin& relying_party_embedder) override;
  void RemoveEmbargoForAutoReauthn(
      const url::Origin& relying_party_embedder) override;
  void SetRequiresUserMediation(const url::Origin& rp_origin,
                                bool requires_user_mediation) override;
  bool RequiresUserMediation(const url::Origin& rp_origin) override;

  // FederatedIdentityPermissionContextDelegate
  void AddIdpSigninStatusObserver(IdpSigninStatusObserver* observer) override;
  void RemoveIdpSigninStatusObserver(
      IdpSigninStatusObserver* observer) override;
  bool HasSharingPermission(const url::Origin& relying_party_requester,
                            const url::Origin& relying_party_embedder,
                            const url::Origin& identity_provider) override;
  std::optional<base::Time> GetLastUsedTimestamp(
      const url::Origin& relying_party_requester,
      const url::Origin& relying_party_embedder,
      const url::Origin& identity_provider,
      const std::string& account_id) override;
  bool HasSharingPermission(
      const url::Origin& relying_party_requester) override;
  void GrantSharingPermission(const url::Origin& relying_party_requester,
                              const url::Origin& relying_party_embedder,
                              const url::Origin& identity_provider,
                              const std::string& account_id) override;
  void RevokeSharingPermission(const url::Origin& relying_party_requester,
                               const url::Origin& relying_party_embedder,
                               const url::Origin& identity_provider,
                               const std::string& account_id) override;
  void RefreshExistingSharingPermission(
      const url::Origin& relying_party_requester,
      const url::Origin& relying_party_embedder,
      const url::Origin& identity_provider,
      const std::string& account_id) override;
  std::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override;
  void SetIdpSigninStatus(const url::Origin& idp_origin,
                          bool idp_signin_status) override;

  void RegisterIdP(const ::GURL&) override;
  void UnregisterIdP(const ::GURL&) override;
  std::vector<GURL> GetRegisteredIdPs() override;
  void OnSetRequiresUserMediation(const url::Origin& relying_party,
                                  base::OnceClosure callback) override;

  void SetIdpStatusClosureForTesting(base::RepeatingClosure closure) {
    idp_signin_status_closure_ = std::move(closure);
  }

  CONTENT_EXPORT void SetHasThirdPartyCookiesAccessForTesting(
      const std::string& identity_provider,
      const std::string& relying_party_embedder);

  CONTENT_EXPORT void ResetForTesting();

 private:
  // Pairs of <RP embedder, IDP>
  std::set<std::pair<std::string, std::string>> request_permissions_;
  // Tuples of <RP requester, RP embedder, IDP, Account>
  std::set<std::tuple<std::string, std::string, std::string, std::string>>
      sharing_permissions_;
  // Map of <IDP, IDPSigninStatus>
  std::map<std::string, std::optional<bool>> idp_signin_status_;
  // Pairs of <IDP, RP embedder>
  std::set<std::pair<std::string, std::string>> has_third_party_cookies_access_;

  base::ObserverList<IdpSigninStatusObserver> idp_signin_status_observer_list_;
  base::RepeatingClosure idp_signin_status_closure_;

  bool auto_reauthn_permission_{true};

  // A vector of registered IdPs.
  std::vector<GURL> idp_registry_;

  // A set of embargoed origins which have a FedCM embargo. An origin is added
  // to the set when the user dismisses the FedCM UI.
  std::set<url::Origin> embargoed_origins_;

  // A set of sites that require user mediation.
  std::set<net::SchemefulSite> require_user_mediation_sites_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_IN_MEMORY_FEDERATED_PERMISSION_CONTEXT_H_
