// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_FEDERATED_PERMISSION_CONTEXT_H_
#define CONTENT_SHELL_BROWSER_SHELL_FEDERATED_PERMISSION_CONTEXT_H_

#include <set>
#include <string>
#include <tuple>

#include "content/public/browser/federated_identity_active_session_permission_context_delegate.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_sharing_permission_context_delegate.h"

namespace content {

// This class implements the various FedCM delegates for content_shell.
// It is used to store permission and login state in memory, so that we
// can run wpt tests against it.
class ShellFederatedPermissionContext
    : public FederatedIdentityApiPermissionContextDelegate,
      public FederatedIdentityActiveSessionPermissionContextDelegate,
      public FederatedIdentitySharingPermissionContextDelegate {
 public:
  ShellFederatedPermissionContext();
  ~ShellFederatedPermissionContext() override;

  // FederatedIdentityApiPermissionContextDelegate
  content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus
  GetApiPermissionStatus(const url::Origin& rp_origin) override;
  void RecordDismissAndEmbargo(const url::Origin& rp_origin) override;
  void RemoveEmbargoAndResetCounts(const url::Origin& rp_origin) override;

  // FederatedIdentitySharingPermissionContextDelegate
  bool HasSharingPermissionForAnyAccount(
      const url::Origin& relying_party,
      const url::Origin& identity_provider) override;
  bool HasSharingPermission(const url::Origin& relying_party,
                            const url::Origin& identity_provider,
                            const std::string& account_id) override;
  void GrantSharingPermission(const url::Origin& relying_party,
                              const url::Origin& identity_provider,
                              const std::string& account_id) override;
  void RevokeSharingPermission(const url::Origin& relying_party,
                               const url::Origin& identity_provider,
                               const std::string& account_id) override;

  // FederatedIdentityActiveSessionPermissionContextDelegate
  bool HasActiveSession(const url::Origin& relying_party,
                        const url::Origin& identity_provider,
                        const std::string& account_identifier) override;
  void GrantActiveSession(const url::Origin& relying_party,
                          const url::Origin& identity_provider,
                          const std::string& account_identifier) override;
  void RevokeActiveSession(const url::Origin& relying_party,
                           const url::Origin& identity_provider,
                           const std::string& account_identifier) override;

  bool ShouldCompleteRequestImmediatelyOnError() const override;

 private:
  // Pairs of <RP, IDP>
  std::set<std::pair<std::string, std::string>> request_permissions_;
  // Tuples of <RP, IDP, Account>
  std::set<std::tuple<std::string, std::string, std::string>>
      sharing_permissions_;
  // Tuples of <RP, IDP, Account>
  std::set<std::tuple<std::string, std::string, std::string>> active_sessions_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_FEDERATED_PERMISSION_CONTEXT_H_
