// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FEDCM_ACCOUNTS_FETCHER_H_
#define CONTENT_BROWSER_FEDCM_ACCOUNTS_FETCHER_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "content/browser/webid/federated_provider_fetcher.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "url/gurl.h"

namespace content {

class FederatedAuthRequestImpl;
class RenderFrameHost;

// A class that fetches accounts from a set of IDPs. Currently only handles
// config and well-known fetches.
// TODO(crbug.com/417197032): handle accounts fetches in this class.
class FedCmAccountsFetcher {
 public:
  struct IdentityProviderGetInfo {
    IdentityProviderGetInfo(blink::mojom::IdentityProviderRequestOptionsPtr,
                            blink::mojom::RpContext rp_context,
                            blink::mojom::RpMode rp_mode,
                            std::optional<blink::mojom::Format> format);
    ~IdentityProviderGetInfo();
    IdentityProviderGetInfo(const IdentityProviderGetInfo&);
    IdentityProviderGetInfo& operator=(const IdentityProviderGetInfo& other);

    blink::mojom::IdentityProviderRequestOptionsPtr provider;
    blink::mojom::RpContext rp_context{blink::mojom::RpContext::kSignIn};
    blink::mojom::RpMode rp_mode{blink::mojom::RpMode::kPassive};
    std::optional<blink::mojom::Format> format;
  };

  FedCmAccountsFetcher(
      RenderFrameHost& render_frame_host,
      IdpNetworkRequestManager* network_manager,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      RpMode rp_mode,
      FederatedAuthRequestImpl* federated_auth_request_impl);
  ~FedCmAccountsFetcher();

  // Fetch well-known, config, accounts and client metadata endpoints for
  // passed-in IdPs. Uses parameters from `token_request_get_infos_`.
  void FetchEndpointsForIdps(const std::set<GURL>& idp_config_urls,
                             int icon_ideal_size,
                             int icon_minimum_size);

 private:
  void OnAllConfigAndWellKnownFetched(
      std::vector<FederatedProviderFetcher::FetchResult> fetch_results);

  std::unique_ptr<FederatedProviderFetcher> provider_fetcher_;

  // Owned by FederatedAuthRequestImpl.
  raw_ref<RenderFrameHost> render_frame_host_;
  raw_ptr<IdpNetworkRequestManager> network_manager_;
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_;

  RpMode rp_mode_;

  // TODO(crbug.com/417197032): Remove this once code has been refactored.
  raw_ptr<FederatedAuthRequestImpl> federated_auth_request_impl_;

  base::WeakPtrFactory<FedCmAccountsFetcher> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FEDCM_ACCOUNTS_FETCHER_H_
