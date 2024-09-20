// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_USER_INFO_REQUEST_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_USER_INFO_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/webid/federated_provider_fetcher.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityPermissionContextDelegate;
class FederatedProviderFetcher;
class RenderFrameHost;

using FederatedAuthUserInfoRequestResult =
    blink::mojom::FederatedAuthUserInfoRequestResult;

// Fetches data for user-info request.
class CONTENT_EXPORT FederatedAuthUserInfoRequest {
 public:
  // Returns an object which fetches data for user-info request.
  static std::unique_ptr<FederatedAuthUserInfoRequest> Create(
      std::unique_ptr<IdpNetworkRequestManager> network_manager,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
      RenderFrameHost* render_frame_host,
      blink::mojom::IdentityProviderConfigPtr provider);

  FederatedAuthUserInfoRequest(const FederatedAuthUserInfoRequest&) = delete;
  FederatedAuthUserInfoRequest& operator=(const FederatedAuthUserInfoRequest&) =
      delete;
  ~FederatedAuthUserInfoRequest();

  // There is a separate method to set the callback because the callback relies
  // on having a pointer to this object, hence cannot be passed in the
  // constructor. Once the callback is set, start fetching.
  void SetCallbackAndStart(
      blink::mojom::FederatedAuthRequest::RequestUserInfoCallback callback);

 private:
  FederatedAuthUserInfoRequest(
      std::unique_ptr<IdpNetworkRequestManager> network_manager,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
      RenderFrameHost* render_frame_host,
      blink::mojom::IdentityProviderConfigPtr provider);

  void OnAllConfigAndWellKnownFetched(
      std::vector<FederatedProviderFetcher::FetchResult> fetch_results);

  void OnAccountsResponseReceived(
      IdpNetworkRequestManager::FetchStatus fetch_status,
      std::vector<IdentityRequestAccountPtr> accounts);

  void MaybeReturnAccounts(
      const std::vector<IdentityRequestAccountPtr>& accounts);

  bool IsReturningAccount(const IdentityRequestAccount& account);

  void Complete(
      blink::mojom::RequestUserInfoStatus status,
      std::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info,
      FederatedAuthUserInfoRequestResult request_status);

  void CompleteWithError(FederatedAuthUserInfoRequestResult error);

  void AddDevToolsIssue(FederatedAuthUserInfoRequestResult error);

  std::unique_ptr<IdpNetworkRequestManager> network_manager_;
  // Owned by |BrowserContext|
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_ =
      nullptr;
  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_delegate_ = nullptr;
  raw_ptr<RenderFrameHost, DanglingUntriaged> render_frame_host_;

  std::unique_ptr<FederatedProviderFetcher> provider_fetcher_;
  bool does_idp_have_failing_signin_status_{false};
  std::string client_id_;
  GURL idp_config_url_;

  url::Origin origin_;
  url::Origin embedding_origin_;
  url::Origin parent_frame_origin_;

  base::TimeTicks request_start_time_;

  blink::mojom::FederatedAuthRequest::RequestUserInfoCallback callback_;

  base::WeakPtrFactory<FederatedAuthUserInfoRequest> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_USER_INFO_REQUEST_H_
