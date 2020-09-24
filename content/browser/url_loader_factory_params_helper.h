// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_URL_LOADER_FACTORY_PARAMS_HELPER_H_
#define CONTENT_BROWSER_URL_LOADER_FACTORY_PARAMS_HELPER_H_

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "url/origin.h"

namespace net {
class IsolationInfo;
}  // namespace net

namespace content {

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
      network::mojom::ClientSecurityStatePtr client_security_state,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      RenderProcessHost* process,
      network::mojom::TrustTokenRedemptionPolicy trust_token_redemption_policy,
      base::StringPiece debug_tag);

  // Creates URLLoaderFactoryParams to be used by |isolated_world_origin| hosted
  // within the |frame|.
  static network::mojom::URLLoaderFactoryParamsPtr CreateForIsolatedWorld(
      RenderFrameHostImpl* frame,
      const url::Origin& isolated_world_origin,
      const url::Origin& main_world_origin,
      network::mojom::ClientSecurityStatePtr client_security_state,
      network::mojom::TrustTokenRedemptionPolicy trust_token_redemption_policy);

  static network::mojom::URLLoaderFactoryParamsPtr CreateForPrefetch(
      RenderFrameHostImpl* frame,
      network::mojom::ClientSecurityStatePtr client_security_state);

  // Creates URLLoaderFactoryParams for either fetching the worker script or for
  // fetches initiated from a worker.
  static network::mojom::URLLoaderFactoryParamsPtr CreateForWorker(
      RenderProcessHost* process,
      const url::Origin& request_initiator,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      base::StringPiece debug_tag);

  // TODO(kinuko, lukasza): https://crbug.com/1114822: Remove, once all
  // URLLoaderFactories vended to a renderer process are associated with a
  // specific origin and an execution context (e.g. a frame, a service worker or
  // any other kind of worker).
  static network::mojom::URLLoaderFactoryParamsPtr CreateForRendererProcess(
      RenderProcessHost* process);

 private:
  // Only static methods.
  URLLoaderFactoryParamsHelper() = delete;
};

}  // namespace content

#endif  // CONTENT_BROWSER_URL_LOADER_FACTORY_PARAMS_HELPER_H_
