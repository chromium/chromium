// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REVOKE_REQUEST_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REVOKE_REQUEST_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/browser/webid/federated_provider_fetcher.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

class FedCmMetrics;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityPermissionContextDelegate;
class FederatedProviderFetcher;
class RenderFrameHost;

// Fetches data for a FedCM revoke request.
class CONTENT_EXPORT FederatedAuthRevokeRequest {
 public:
  // Returns an object which fetches data for revoke request.
  static std::unique_ptr<FederatedAuthRevokeRequest> Create(
      std::unique_ptr<IdpNetworkRequestManager> network_manager,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      RenderFrameHost* render_frame_host,
      FedCmMetrics* metrics,
      blink::mojom::IdentityCredentialRevokeOptionsPtr options,
      bool should_complete_request_immediately);

  FederatedAuthRevokeRequest(const FederatedAuthRevokeRequest&) = delete;
  FederatedAuthRevokeRequest& operator=(const FederatedAuthRevokeRequest&) =
      delete;
  ~FederatedAuthRevokeRequest();

  // There is a separate method to set the callback because the callback relies
  // on having a pointer to this object, hence cannot be passed in the
  // constructor. Once the callback is set, start fetching.
  void SetCallbackAndStart(
      blink::mojom::FederatedAuthRequest::RevokeCallback callback,
      FederatedIdentityApiPermissionContextDelegate* api_permission_delegate);

 private:
  FederatedAuthRevokeRequest(
      std::unique_ptr<IdpNetworkRequestManager> network_manager,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      RenderFrameHost* render_frame_host,
      FedCmMetrics* metrics,
      blink::mojom::IdentityCredentialRevokeOptionsPtr options,
      bool should_complete_request_immediately);

  void OnAllConfigAndWellKnownFetched(
      std::vector<FederatedProviderFetcher::FetchResult> fetch_results);

  void OnRevokeResponse(IdpNetworkRequestManager::RevokeResponse response);

  // `should_delay_callback` represents whether we should call the callback
  // with some delay or immediately. For some failures we choose to reject
  // with some delay for privacy reasons. `revoke_status_for_metrics` is
  // non-nullopt if metrics have not yet been recorded for this request.
  void Complete(
      blink::mojom::RevokeStatus status,
      absl::optional<content::FedCmRevokeStatus> revoke_status_for_metrics,
      bool should_delay_callback);

  std::unique_ptr<IdpNetworkRequestManager> network_manager_;
  // Owned by |BrowserContext|
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_ =
      nullptr;
  // Owned by |FederatedAuthRequestImpl|
  raw_ptr<FedCmMetrics> metrics_;
  raw_ptr<RenderFrameHost, DanglingUntriaged> render_frame_host_;

  std::unique_ptr<FederatedProviderFetcher> provider_fetcher_;
  blink::mojom::IdentityCredentialRevokeOptionsPtr options_;
  bool should_complete_request_immediately_{false};

  url::Origin origin_;
  url::Origin embedding_origin_;

  blink::mojom::FederatedAuthRequest::RevokeCallback callback_;

  base::WeakPtrFactory<FederatedAuthRevokeRequest> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REVOKE_REQUEST_H_
