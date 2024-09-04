// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader_service_context.h"

#include "content/browser/loader/prefetch_url_loader.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace content {

PrefetchURLLoaderServiceContext::PrefetchURLLoaderServiceContext(
    BrowserContext* browser_context,
    mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                      scoped_refptr<BindContext>>& loader_factory_receivers)
    : browser_context_(browser_context),
      loader_factory_receivers_(loader_factory_receivers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  accept_langs_ =
      GetContentClient()->browser()->GetAcceptLangs(browser_context);

  // Create a RendererPreferenceWatcher to observe updates in the preferences.
  GetContentClient()->browser()->RegisterRendererPreferenceWatcher(
      browser_context, preference_watcher_receiver_.BindNewPipeAndPassRemote());
}

void PrefetchURLLoaderServiceContext::CreatePrefetchLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request_in,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  CHECK(IsPrefetchRequest(resource_request_in));

  // Make a copy of |resource_request_in|, because we may need to modify the
  // request.
  network::ResourceRequest resource_request = resource_request_in;

  BindContext& current_context = *current_bind_context();

  if (!current_context.render_frame_host) {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_to_use =
      current_context.factory;

  // The request must not already have its `trusted_params` initialized.
  if (resource_request.trusted_params) {
    loader_factory_receivers_->ReportBadMessage(
        "Prefetch/CreatePrefetchLoaderAndStart: trusted params");
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(
            network::URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  if (resource_request.load_flags &
      net::LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME) {
    CHECK(!resource_request.recursive_prefetch_token);
    // The renderer has marked this prefetch as restricted, meaning it is a
    // cross-origin prefetch intended for top-level navigation reuse. We must
    // verify that the request meets the necessary security requirements, and
    // populate `resource_request`'s IsolationInfo appropriately.
    EnsureCrossOriginFactory();
    DCHECK(current_context.cross_origin_factory);

    // An invalid request could indicate a compromised renderer
    // inappropriately modifying the request, so we immediately complete it
    // with an error.
    if (!IsValidCrossOriginPrefetch(resource_request)) {
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
      return;
    }

    // Cross-site prefetches shouldn't include SameSite cookies.
    resource_request.site_for_cookies = net::SiteForCookies();

    // Attach the fenced frame nonce to the request's IsolationInfo. If the
    // nonce is marked revoked for untrusted network access, the request will
    // either not be created due to the check in
    // `CorsURLLoaderFactory::CreateLoaderAndStart` or cancelled due to the
    // check `CancelRequestIfNonceMatchesAndUrlNotExempted`.
    // TODO(crbug.com/349978810): Attach credentialless iframe nonce.
    std::optional<base::UnguessableToken> fenced_frame_nonce =
        current_context.render_frame_host->frame_tree_node()
            ->GetFencedFrameNonce();

    // Use the trusted cross-origin prefetch loader factory, and set the
    // request's IsolationInfo suitable for the cross-origin prefetch.
    network_loader_factory_to_use = current_context.cross_origin_factory;
    url::Origin destination_origin = url::Origin::Create(resource_request.url);
    resource_request.trusted_params = network::ResourceRequest::TrustedParams();
    resource_request.trusted_params->isolation_info =
        net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                   destination_origin, destination_origin,
                                   net::SiteForCookies(),
                                   /*nonce=*/fenced_frame_nonce);
  } else if (resource_request.recursive_prefetch_token) {
    // Recursive prefetch from a cross-origin main resource prefetch.

    // TODO(crbug.com/40150754): Figure out why we're seeing this condition
    // hold true in the field.
    if (!current_context.cross_origin_factory) {
      return;
    }

    // Resurrect the request's IsolationInfo from the current context's map,
    // and use it for this request.
    auto isolation_info_iterator =
        current_context.prefetch_isolation_infos.find(
            resource_request.recursive_prefetch_token.value());

    // An unexpected token could indicate a compromised renderer trying to
    // fetch a request in a special way. We'll cancel the request.
    if (isolation_info_iterator ==
        current_context.prefetch_isolation_infos.end()) {
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
      return;
    }

    // Cross-site prefetches shouldn't include SameSite cookies.
    resource_request.site_for_cookies = net::SiteForCookies();

    resource_request.trusted_params = network::ResourceRequest::TrustedParams();
    resource_request.trusted_params->isolation_info =
        isolation_info_iterator->second;
    network_loader_factory_to_use = current_context.cross_origin_factory;
  }

  if (prefetch_load_callback_for_testing_) {
    prefetch_load_callback_for_testing_.Run();
  }

  scoped_refptr<PrefetchedSignedExchangeCache>
      prefetched_signed_exchange_cache =
          current_context.prefetched_signed_exchange_cache;

  // base::Unretained is safe here since |this| owns the loader.
  auto loader = std::make_unique<PrefetchURLLoader>(
      request_id, options, current_context.frame_tree_node_id, resource_request,
      resource_request.trusted_params
          ? resource_request.trusted_params->isolation_info
                .network_anonymization_key()
          : current_context.render_frame_host->GetIsolationInfoForSubresources()
                .network_anonymization_key(),
      std::move(client), traffic_annotation,
      std::move(network_loader_factory_to_use),
      base::BindRepeating(
          &PrefetchURLLoaderServiceContext::CreateURLLoaderThrottles,
          base::Unretained(this), resource_request,
          current_context.frame_tree_node_id),
      browser_context_, std::move(prefetched_signed_exchange_cache),
      accept_langs_,
      base::BindOnce(
          &PrefetchURLLoaderServiceContext::GenerateRecursivePrefetchToken,
          base::Unretained(this),
          current_context.weak_ptr_factory.GetWeakPtr()));
  auto* raw_loader = loader.get();
  prefetch_loader_receivers_.Add(raw_loader, std::move(receiver),
                                 std::move(loader));
}

PrefetchURLLoaderServiceContext::~PrefetchURLLoaderServiceContext() = default;

// This method is used to determine whether it is safe to set the IsolationInfo
// of a cross-origin prefetch request coming from the renderer, so that it can
// be cached correctly.
bool PrefetchURLLoaderServiceContext::IsValidCrossOriginPrefetch(
    const network::ResourceRequest& resource_request) {
  // All fetches need to have an associated request_initiator.
  if (!resource_request.request_initiator) {
    loader_factory_receivers_->ReportBadMessage(
        "Prefetch/IsValidCrossOrigin: no request_initiator");
    return false;
  }

  // The request is expected to be cross-origin. Same-origin prefetches do not
  // need a special IsolationInfo, and therefore must not be marked for
  // restricted use.
  if (resource_request.request_initiator->IsSameOriginWith(
          resource_request.url)) {
    loader_factory_receivers_->ReportBadMessage(
        "Prefetch/IsValidCrossOrigin: same-origin");
    return false;
  }

  // The request initiator has to match the request_initiator_origin_lock - it
  // has to be the same origin as the last committed origin in the frame.
  const BindContext& current_context = *current_bind_context();
  // Presence of |render_frame_host| is guaranteed by the caller - the caller
  // calls earlier EnsureCrossOriginFactory which has the same DCHECK.
  DCHECK(current_context.render_frame_host);
  if (!resource_request.request_initiator->opaque() &&
      resource_request.request_initiator.value() !=
          current_context.render_frame_host->GetLastCommittedOrigin()) {
    loader_factory_receivers_->ReportBadMessage(
        "Prefetch/IsValidCrossOrigin: frame origin mismatch");
    return false;
  }

  // If the PrefetchPrivacyChanges feature is enabled, the request's redirect
  // mode must be |kError|.
  if (base::FeatureList::IsEnabled(blink::features::kPrefetchPrivacyChanges) &&
      resource_request.redirect_mode != network::mojom::RedirectMode::kError) {
    loader_factory_receivers_->ReportBadMessage(
        "Prefetch/IsValidCrossOrigin: wrong redirect mode");
    return false;
  }

  // This prefetch request must not be able to reuse restricted prefetches from
  // the prefetch cache. This is because it is possible that another origin
  // prefetched the same resource, which should only be reused for top-level
  // navigations.
  if (resource_request.load_flags &
      net::LOAD_CAN_USE_RESTRICTED_PREFETCH_FOR_MAIN_FRAME) {
    loader_factory_receivers_->ReportBadMessage(
        "Prefetch/IsValidCrossOrigin: can use restricted prefetch");
    return false;
  }

  return true;
}

void PrefetchURLLoaderServiceContext::EnsureCrossOriginFactory() {
  BindContext& current_context = *current_bind_context();
  // If the factory has already been created, don't re-create it.
  if (current_context.cross_origin_factory) {
    return;
  }

  DCHECK(current_context.render_frame_host);
  std::unique_ptr<network::PendingSharedURLLoaderFactory> factories =
      current_context.render_frame_host
          ->CreateCrossOriginPrefetchLoaderFactoryBundle();
  current_context.cross_origin_factory =
      network::SharedURLLoaderFactory::Create(std::move(factories));
}

void PrefetchURLLoaderServiceContext::NotifyUpdate(
    const blink::RendererPreferences& new_prefs) {
  SetAcceptLanguages(new_prefs.accept_languages);
}

base::UnguessableToken
PrefetchURLLoaderServiceContext::GenerateRecursivePrefetchToken(
    base::WeakPtr<BindContext> current_context,
    const network::ResourceRequest& request) {
  // If the relevant frame has gone away before this method is called
  // asynchronously, we cannot generate and store a {token, IsolationInfo} pair
  // in the frame's `prefetch_isolation_infos` map, so we'll create and return a
  // dummy token that will not get used.
  if (!current_context) {
    return base::UnguessableToken::Create();
  }

  // Attach the fenced frame nonce to the request's IsolationInfo. If the nonce
  // is marked revoked for untrusted network access, the request will either not
  // be created due to the check in `CorsURLLoaderFactory::CreateLoaderAndStart`
  // or cancelled due to the check
  // `CancelRequestIfNonceMatchesAndUrlNotExempted`.
  // TODO(crbug.com/349978810): Attach credentialless iframe nonce.
  std::optional<base::UnguessableToken> fenced_frame_nonce =
      current_context->render_frame_host
          ? current_context->render_frame_host->frame_tree_node()
                ->GetFencedFrameNonce()
          : std::nullopt;

  // Create IsolationInfo.
  url::Origin destination_origin = url::Origin::Create(request.url);
  net::IsolationInfo preload_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, destination_origin,
      destination_origin, net::SiteForCookies(), /*nonce=*/fenced_frame_nonce);

  // Generate token.
  base::UnguessableToken return_token = base::UnguessableToken::Create();

  // Associate the two, and return the token.
  current_context->prefetch_isolation_infos.insert(
      {return_token, preload_isolation_info});
  return return_token;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
PrefetchURLLoaderServiceContext::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    FrameTreeNodeId frame_tree_node_id) {
  return CreateContentBrowserURLLoaderThrottles(
      request, browser_context_,
      base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                          frame_tree_node_id),
      nullptr /* navigation_ui_data */, frame_tree_node_id,
      /*navigation_id=*/std::nullopt);
}

}  // namespace content
