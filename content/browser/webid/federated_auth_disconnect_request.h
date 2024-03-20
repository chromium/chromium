// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_DISCONNECT_REQUEST_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_DISCONNECT_REQUEST_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/browser/webid/federated_provider_fetcher.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

class FedCmMetrics;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityPermissionContextDelegate;
class FederatedProviderFetcher;
class RenderFrameHost;

// Fetches data for a FedCM disconnect request.
class CONTENT_EXPORT FederatedAuthDisconnectRequest {
 public:
  // Returns an object which fetches data for disconnect request.
  static std::unique_ptr<FederatedAuthDisconnectRequest> Create(
      std::unique_ptr<IdpNetworkRequestManager> network_manager,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      RenderFrameHost* render_frame_host,
      FedCmMetrics* metrics,
      blink::mojom::IdentityCredentialDisconnectOptionsPtr options);

  FederatedAuthDisconnectRequest(const FederatedAuthDisconnectRequest&) =
      delete;
  FederatedAuthDisconnectRequest& operator=(
      const FederatedAuthDisconnectRequest&) = delete;
  ~FederatedAuthDisconnectRequest();

  // There is a separate method to set the callback because the callback relies
  // on having a pointer to this object, hence cannot be passed in the
  // constructor. Once the callback is set, start fetching.
  void SetCallbackAndStart(
      blink::mojom::FederatedAuthRequest::DisconnectCallback callback,
      FederatedIdentityApiPermissionContextDelegate* api_permission_delegate);

 private:
  FederatedAuthDisconnectRequest(
      std::unique_ptr<IdpNetworkRequestManager> network_manager,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      RenderFrameHost* render_frame_host,
      FedCmMetrics* metrics,
      blink::mojom::IdentityCredentialDisconnectOptionsPtr options);

  void OnAllConfigAndWellKnownFetched(
      std::vector<FederatedProviderFetcher::FetchResult> fetch_results);

  void OnDisconnectResponse(IdpNetworkRequestManager::FetchStatus fetch_status,
                            const std::string& account_id);

  // Records disconnect metrics and completes the request.
  void Complete(blink::mojom::DisconnectStatus status,
                content::FedCmDisconnectStatus disconnect_status_for_metrics);

  void AddConsoleErrorMessage(
      FedCmDisconnectStatus disconnect_status_for_metrics);

  std::unique_ptr<IdpNetworkRequestManager> network_manager_;
  // Owned by |BrowserContext|
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_ =
      nullptr;
  // Owned by |FederatedAuthRequestImpl|
  raw_ptr<FedCmMetrics> metrics_;
  raw_ptr<RenderFrameHost, DanglingUntriaged> render_frame_host_;

  std::unique_ptr<FederatedProviderFetcher> provider_fetcher_;
  blink::mojom::IdentityCredentialDisconnectOptionsPtr options_;

  url::Origin origin_;
  url::Origin embedding_origin_;

  blink::mojom::FederatedAuthRequest::DisconnectCallback callback_;

  // The time when this class is created. Approximates the time in which the
  // disconnect() call begins.
  base::TimeTicks start_time_;
  // Whether the disconnect fetch request is sent. Used to know whether to
  // record the disconnect call duration.
  bool disconnect_request_sent_ = false;

  base::WeakPtrFactory<FederatedAuthDisconnectRequest> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_DISCONNECT_REQUEST_H_
