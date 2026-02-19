// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDENTITY_CREDENTIAL_SOURCE_IMPL_H_
#define CONTENT_BROWSER_WEBID_IDENTITY_CREDENTIAL_SOURCE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/browser/webid/accounts_fetcher.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/browser/webid/metrics.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/webid/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/webid/federated_identity_permission_context_delegate.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHost;

namespace webid {
class CONTENT_EXPORT IdentityCredentialSourceImpl
    : public DocumentUserData<IdentityCredentialSourceImpl>,
      public IdentityCredentialSource {
 public:
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit IdentityCredentialSourceImpl(RenderFrameHost* rfh);
  ~IdentityCredentialSourceImpl() override;

  void GetIdentityCredentialSuggestions(
      const std::vector<GURL>& embedder_requested_idps,
      GetIdentityCredentialSuggestionsCallback callback) override;

  bool SelectAccount(const url::Origin& idp_origin,
                     const std::string& account_id) override;

  void SetNetworkManagerForTests(
      std::unique_ptr<IdpNetworkRequestManager> network_manager);
  void SetPermissionDelegateForTests(
      FederatedIdentityPermissionContextDelegate* permission_delegate);

 private:
  void OnAccountsFetchCompleted(base::TimeTicks,
                                std::vector<AccountsFetcher::Result> results);

  std::unique_ptr<IdpNetworkRequestManager> network_manager_;
  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_ =
      nullptr;

  std::unique_ptr<Metrics> metrics_;
  std::unique_ptr<AccountsFetcher> accounts_fetcher_;
  GetIdentityCredentialSuggestionsCallback callback_;

  base::WeakPtrFactory<IdentityCredentialSourceImpl> weak_ptr_factory_{this};
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDENTITY_CREDENTIAL_SOURCE_IMPL_H_
