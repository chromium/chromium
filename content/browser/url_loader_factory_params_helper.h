// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_URL_LOADER_FACTORY_PARAMS_HELPER_H_
#define CONTENT_BROWSER_URL_LOADER_FACTORY_PARAMS_HELPER_H_

#include <string_view>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-forward.h"
#include "services/network/public/mojom/early_hints.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "url/origin.h"

namespace net {
class IsolationInfo;
}  // namespace net

namespace network {
namespace mojom {
class SharedDictionaryAccessObserver;
}  // namespace mojom
}  // namespace network

namespace content {

class NavigationRequest;
class RenderFrameHostImpl;
class RenderProcessHost;

// URLLoaderFactoryParamsHelper encapsulates details of how to create
// network::mojom::URLLoaderFactoryParams (taking //content-focused parameters,
// calling into ContentBrowserClient's OverrideURLLoaderFactoryParams method,
// etc.)
class URLLoaderFactoryParamsHelper {
 public:
  // Creates URLLoaderFactoryParams for a factory to be used from |process|,
  // with parameters controlled by |frame| and |origin|.
  //
  // This overload is used to create a factory for:
  // - fetching subresources from the |frame|
  // - fetching subresources from a dedicated worker associated with the |frame|
  // - fetching main worker script (when the worker is created by the |frame|)
  //
  // |origin| is exposed as a separate parameter, to accommodate calls during
  // ready-to-commit time (when |frame|'s GetLastCommittedOrigin has not been
  // updated yet).
  //
  // |process| is exposed as a separate parameter, to accommodate creating
  // factories for dedicated workers (where the |process| hosting the worker
  // might be different from the process hosting the |frame|).
  CONTENT_EXPORT static network::mojom::URLLoaderFactoryParamsPtr
  CreateForFrame(
      RenderFrameHostImpl* frame,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      network::mojom::ClientSecurityStatePtr client_security_state,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      RenderProcessHost* process,
      network::mojom::TrustTokenOperationPolicyVerdict
          trust_token_issuance_policy,
      network::mojom::TrustTokenOperationPolicyVerdict
          trust_token_redemption_policy,
      net::CookieSettingOverrides cookie_setting_overrides,
      std::string_view debug_tag);

  // Creates URLLoaderFactoryParams to be used by |isolated_world_origin| hosted
  // within the |frame|.
  //
  // TODO(crbug.com/40137011): Remove the CreateForIsolatedWorld method
  // once Chrome Platform Apps are gone.
  static network::mojom::URLLoaderFactoryParamsPtr CreateForIsolatedWorld(
      RenderFrameHostImpl* frame,
      const url::Origin& isolated_world_origin,
      const url::Origin& main_world_origin,
      const net::IsolationInfo& isolation_info,
      network::mojom::ClientSecurityStatePtr client_security_state,
      network::mojom::TrustTokenOperationPolicyVerdict
          trust_token_issuance_policy,
      network::mojom::TrustTokenOperationPolicyVerdict
          trust_token_redemption_policy,
      net::CookieSettingOverrides cookie_setting_overrides);

  static network::mojom::URLLoaderFactoryParamsPtr CreateForPrefetch(
      RenderFrameHostImpl* frame,
      network::mojom::ClientSecurityStatePtr client_security_state,
      net::CookieSettingOverrides cookie_setting_overrides);

  // Creates URLLoaderFactoryParams for either fetching the worker script or for
  // fetches initiated from a worker.
  static network::mojom::URLLoaderFactoryParamsPtr CreateForWorker(
      RenderProcessHost* process,
      const url::Origin& request_initiator,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer,
      network::mojom::ClientSecurityStatePtr client_security_state,
      std::string_view debug_tag,
      bool require_cross_site_request_for_cookies);

  // Creates URLLoaderFactoryParams for Early Hints preload.
  // When a redirect happens, a URLLoaderFactory created from the
  // URLLoaderFactoryParams must be destroyed since some parameters are
  // calculated from speculative state of `navigation_request`.
  static network::mojom::URLLoaderFactoryParamsPtr CreateForEarlyHintsPreload(
      RenderProcessHost* process,
      const url::Origin& tentative_origin,
      NavigationRequest& navigation_request,
      const network::mojom::EarlyHints& early_hints,
      mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
      mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
          trust_token_observer,
      mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
          shared_dictionary_observer);

 private:
  // Only static methods.
  URLLoaderFactoryParamsHelper() = delete;
};

}  // namespace content

#endif  // CONTENT_BROWSER_URL_LOADER_FACTORY_PARAMS_HELPER_H_
