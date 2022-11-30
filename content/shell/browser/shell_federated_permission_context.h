// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_FEDERATED_PERMISSION_CONTEXT_H_
#define CONTENT_SHELL_BROWSER_SHELL_FEDERATED_PERMISSION_CONTEXT_H_

#include <map>
#include <set>
#include <string>
#include <tuple>

#include "base/functional/callback.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"

namespace content {

// This class implements the various FedCM delegates for content_shell.
// It is used to store permission and login state in memory, so that we
// can run wpt tests against it.
class ShellFederatedPermissionContext
    : public FederatedIdentityApiPermissionContextDelegate,
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

  // FederatedIdentityPermissionContextDelegate
  bool HasActiveSession(const url::Origin& relying_party_requester,
                        const url::Origin& identity_provider,
                        const std::string& account_identifier) override;
  void GrantActiveSession(const url::Origin& relying_party_requester,
                          const url::Origin& identity_provider,
                          const std::string& account_identifier) override;
  void RevokeActiveSession(const url::Origin& relying_party_requester,
                           const url::Origin& identity_provider,
                           const std::string& account_identifier) override;
  bool HasSharingPermission(const url::Origin& relying_party_requester,
                            const url::Origin& relying_party_embedder,
                            const url::Origin& identity_provider,
                            const std::string& account_id) override;
  void GrantSharingPermission(const url::Origin& relying_party_requester,
                              const url::Origin& relying_party_embedder,
                              const url::Origin& identity_provider,
                              const std::string& account_id) override;
  absl::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override;
  void SetIdpSigninStatus(const url::Origin& idp_origin,
                          bool idp_signin_status) override;

  void SetIdpStatusClosureForTesting(base::RepeatingClosure closure) {
    idp_signin_status_closure_ = std::move(closure);
  }

 private:
  // Pairs of <RP embedder, IDP>
  std::set<std::pair<std::string, std::string>> request_permissions_;
  // Tuples of <RP requester, RP embedder, IDP, Account>
  std::set<std::tuple<std::string, std::string, std::string, std::string>>
      sharing_permissions_;
  // Tuples of <RP requester, IDP, Account>
  std::set<std::tuple<std::string, std::string, std::string>> active_sessions_;
  // Map of <IDP, IDPSigninStatus>
  std::map<std::string, absl::optional<bool>> idp_signin_status_;

  base::RepeatingClosure idp_signin_status_closure_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_FEDERATED_PERMISSION_CONTEXT_H_
