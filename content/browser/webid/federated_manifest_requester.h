// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_MANIFEST_REQUESTER_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_MANIFEST_REQUESTER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

namespace content {

// Fetches the manifests and manifest lists for a list of identity providers.
// Validates returned information and calls callback when done.
class FederatedManifestRequester {
 public:
  struct FetchError {
    FetchError(const FetchError& info);
    FetchError(blink::mojom::FederatedAuthRequestResult result,
               FedCmRequestIdTokenStatus token_status,
               absl::optional<std::string> additional_console_error_message);
    ~FetchError();

    blink::mojom::FederatedAuthRequestResult result;
    FedCmRequestIdTokenStatus token_status;
    absl::optional<std::string> additional_console_error_message;
  };

  struct FetchResult {
    FetchResult();
    FetchResult(const FetchResult&);
    ~FetchResult();
    GURL identity_provider_config_url;
    IdpNetworkRequestManager::Endpoints endpoints;
    absl::optional<IdentityProviderMetadata> metadata;
    absl::optional<FetchError> error;
  };

  using RequesterCallback = base::OnceCallback<void(std::vector<FetchResult>)>;

  explicit FederatedManifestRequester(
      IdpNetworkRequestManager* network_manager);
  ~FederatedManifestRequester();

  FederatedManifestRequester(const FederatedManifestRequester&) = delete;
  FederatedManifestRequester& operator=(const FederatedManifestRequester&) =
      delete;

  // Starts fetch of manifests and manifest lists. Start() should be called at
  // most once per FederatedManifestRequester instance.
  void Start(const std::vector<GURL>& identity_provider_config_urls,
             int icon_ideal_size,
             int icon_minimum_size,
             RequesterCallback callback);

 private:
  void OnManifestListFetched(FetchResult& fetch_result,
                             IdpNetworkRequestManager::FetchStatus status,
                             const std::set<GURL>& urls);
  void OnManifestFetched(FetchResult& fetch_result,
                         IdpNetworkRequestManager::FetchStatus status,
                         IdpNetworkRequestManager::Endpoints,
                         IdentityProviderMetadata idp_metadata);

  // Called when fetching either the manifest endpoint or the manifest list
  // endpoint fails.
  void OnError(FetchResult& fetch_result,
               blink::mojom::FederatedAuthRequestResult result,
               content::FedCmRequestIdTokenStatus token_status,
               absl::optional<std::string> additional_console_error_message);

  void RunCallbackIfDone();

  RequesterCallback callback_;

  // Config endpoints which has not yet been fetched.
  base::flat_set<GURL> pending_manifest_fetches_;

  // Config endpoints for which associated manifest list has not yet been
  // fetched.
  base::flat_set<GURL> pending_manifest_list_fetches_;

  std::vector<FetchResult> fetch_results_;

  // Fetches the manifest and manifest list.
  base::raw_ptr<IdpNetworkRequestManager, DanglingUntriaged> network_manager_;

  base::WeakPtrFactory<FederatedManifestRequester> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_MANIFEST_REQUESTER_H_
