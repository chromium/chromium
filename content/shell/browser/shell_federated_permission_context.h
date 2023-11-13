// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_FEDERATED_PERMISSION_CONTEXT_H_
#define CONTENT_SHELL_BROWSER_SHELL_FEDERATED_PERMISSION_CONTEXT_H_

#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_auto_reauthn_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "url/gurl.h"

namespace content {

// This class implements the various FedCM delegates for content_shell.
// It is used to store permission and login state in memory, so that we
// can run wpt tests against it.
class ShellFederatedPermissionContext
    : public FederatedIdentityApiPermissionContextDelegate,
      public FederatedIdentityAutoReauthnPermissionContextDelegate,
      public FederatedIdentityPermissionContextDelegate {
 public:
  ShellFederatedPermissionContext();
  ~ShellFederatedPermissionContext() override;

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
  void SetRequiresUserMediation(const GURL& rp_url,
                                bool requires_user_mediation) override;
  bool RequiresUserMediation(const GURL& rp_url) override;

  // FederatedIdentityPermissionContextDelegate
  void AddIdpSigninStatusObserver(IdpSigninStatusObserver* observer) override;
  void RemoveIdpSigninStatusObserver(
      IdpSigninStatusObserver* observer) override;
  bool HasSharingPermission(
      const url::Origin& relying_party_requester,
      const url::Origin& relying_party_embedder,
      const url::Origin& identity_provider,
      const absl::optional<std::string>& account_id) override;
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
  absl::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override;
  void SetIdpSigninStatus(const url::Origin& idp_origin,
                          bool idp_signin_status) override;

  void RegisterIdP(const ::GURL&) override;
  void UnregisterIdP(const ::GURL&) override;
  std::vector<GURL> GetRegisteredIdPs() override;

  void SetIdpStatusClosureForTesting(base::RepeatingClosure closure) {
    idp_signin_status_closure_ = std::move(closure);
  }

  void SetHasThirdPartyCookiesAccessForTesting(
      const std::string& identity_provider,
      const std::string& relying_party_embedder);

 private:
  // Pairs of <RP embedder, IDP>
  std::set<std::pair<std::string, std::string>> request_permissions_;
  // Tuples of <RP requester, RP embedder, IDP, Account>
  std::set<std::tuple<std::string, std::string, std::string, std::string>>
      sharing_permissions_;
  // Map of <IDP, IDPSigninStatus>
  std::map<std::string, absl::optional<bool>> idp_signin_status_;
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

  // A set of urls that require user mediation.
  std::set<GURL> require_user_mediation_sites_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_FEDERATED_PERMISSION_CONTEXT_H_
