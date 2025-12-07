// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_CONFIG_FETCHER_H_
#define CONTENT_BROWSER_WEBID_CONFIG_FETCHER_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/browser/webid/metrics.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

namespace content {
class RenderFrameHost;

namespace webid {

// Fetches the config and well-known files for a list of identity providers.
// Validates returned information and calls callback when done.
class CONTENT_EXPORT ConfigFetcher {
 public:
  struct CONTENT_EXPORT FetchError {
    FetchError(const FetchError& info);
    FetchError(blink::mojom::FederatedAuthRequestResult result,
               RequestIdTokenStatus token_status,
               std::optional<std::string> additional_console_error_message);
    ~FetchError();

    blink::mojom::FederatedAuthRequestResult result;
    RequestIdTokenStatus token_status;
    std::optional<std::string> additional_console_error_message;
  };

  struct CONTENT_EXPORT FetchResult {
    FetchResult();
    FetchResult(const FetchResult&);
    ~FetchResult();
    GURL identity_provider_config_url;
    bool force_skip_well_known_enforcement = false;
    IdpNetworkRequestManager::WellKnown wellknown;
    IdpNetworkRequestManager::Endpoints endpoints;
    std::optional<IdentityProviderMetadata> metadata;
    std::optional<FetchError> error;
  };

  struct FetchRequest {
    GURL identity_provider_config_url;
    bool force_skip_well_known_enforcement = false;
    FetchRequest(const GURL& url, bool force_skip_well_known_enforcement)
        : identity_provider_config_url(url),
          force_skip_well_known_enforcement(force_skip_well_known_enforcement) {
    }
  };

  using RequesterCallback = base::OnceCallback<void(std::vector<FetchResult>)>;

  // TODO(crbug.com/40283354): Remove |render_frame_host| when the IDP signin
  // status API is enabled by default.
  ConfigFetcher(RenderFrameHost& render_frame_host,
                IdpNetworkRequestManager* network_manager);
  ~ConfigFetcher();

  ConfigFetcher(const ConfigFetcher&) = delete;
  ConfigFetcher& operator=(const ConfigFetcher&) = delete;

  // Starts fetch of config and well-known files. Start() should be called at
  // most once per ConfigFetcher instance.
  void Start(const std::vector<FetchRequest>& requested_providers,
             blink::mojom::RpMode rp_mode,
             int icon_ideal_size,
             int icon_minimum_size,
             RequesterCallback callback);

  // Given a FetchResult, validates all of the conditions that the config file
  // and the well-known files need to meet. Sets an "error" in the result in
  // case the validation fails.
  void ValidateAndMaybeSetError(FetchResult& result);

 private:
  void OnWellKnownFetched(
      FetchResult& fetch_result,
      FetchStatus status,
      const IdpNetworkRequestManager::WellKnown& well_known);
  void OnConfigFetched(FetchResult& fetch_result,
                       FetchStatus status,
                       IdpNetworkRequestManager::Endpoints endpoints,
                       IdentityProviderMetadata idp_metadata);

  // Called when fetching either the config endpoint or the well-known
  // endpoint fails.
  void OnError(FetchResult& fetch_result,
               blink::mojom::FederatedAuthRequestResult result,
               RequestIdTokenStatus token_status,
               std::optional<std::string> additional_console_error_message);

  void RunCallbackIfDone();

  bool ShouldSkipWellKnownEnforcementForIdp(const FetchResult& fetch_result);

  raw_ref<RenderFrameHost> render_frame_host_;

  RequesterCallback callback_;

  // Config endpoints which has not yet been fetched.
  base::flat_set<GURL> pending_config_fetches_;

  // Config endpoints for which associated well-known has not yet been
  // fetched.
  base::flat_set<GURL> pending_well_known_fetches_;

  std::vector<FetchResult> fetch_results_;

  // Fetches the config and well-known files.
  raw_ptr<IdpNetworkRequestManager, DanglingUntriaged> network_manager_;

  base::WeakPtrFactory<ConfigFetcher> weak_ptr_factory_{this};
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_CONFIG_FETCHER_H_
