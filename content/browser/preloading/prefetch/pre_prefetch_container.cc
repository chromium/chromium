// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/pre_prefetch_container.h"

#include "base/feature_list.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_resource_request_utils.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace content {

// ------------------------------------------------------------------------
// Methods to be called from the `PrePrefetchServiceCore` sequence.

// static
std::unique_ptr<PrePrefetchContainer> PrePrefetchContainer::CreateAndStart(
    base::PassKey<PrePrefetchServiceImpl>,
    std::unique_ptr<const PrefetchRequest> prefetch_request,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory) {
  // This should only be called from `PrePrefetchServiceCore` sequence through
  // `PrePrefetchServiceImpl`.
  return CreateAndStartInternal(std::move(prefetch_request),
                                std::move(url_loader_factory));
}

// static
std::unique_ptr<PrePrefetchContainer>
PrePrefetchContainer::CreateAndStartForTesting(  // IN-TEST
    std::unique_ptr<const PrefetchRequest> prefetch_request,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory) {
  return CreateAndStartInternal(std::move(prefetch_request),
                                std::move(url_loader_factory));
}

// static
std::unique_ptr<PrePrefetchContainer>
PrePrefetchContainer::CreateAndStartInternal(
    std::unique_ptr<const PrefetchRequest> prefetch_request,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory) {
  auto container = std::make_unique<PrePrefetchContainer>(
      base::PassKey<PrePrefetchContainer>(), std::move(prefetch_request));
  container->Start(std::move(url_loader_factory));
  return container;
}

PrePrefetchContainer::PrePrefetchContainer(
    base::PassKey<PrePrefetchContainer>,
    std::unique_ptr<const PrefetchRequest> prefetch_request)
    : prefetch_request_(std::move(prefetch_request)) {
  // This should only be called from `PrePrefetchServiceCore` sequence through
  // `PrePrefetchServiceImpl`, or the sequence used on the test.
  CHECK(!BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
}

void PrePrefetchContainer::Start(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory) {
  CHECK(!url_loader_);
  CHECK(!url_loader_client_receiver_);

  if (!url_loader_factory) {
    return;
  }

  // TODO(crbug.com/452389538): Perform prefetch eligibility checks that are
  // required before starting PrePrefetch's network request.

  // TODO(crbug.com/452389538): Construct a proper ResourceRequest for
  // PrePrefetch, using the prefetch common utility.
  // We should also take care of the properties that are UI-thread dependent /
  // that are tied to the embedder (See the comment of
  // `PrePrefetchServiceImpl::ctor`).
  const GURL& url = prefetch_request_->key().url();
  url::Origin origin = url::Origin::Create(url);
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, /*top_frame_origin=*/origin,
      /*frame_origin=*/origin, net::SiteForCookies::FromOrigin(origin));
  resource_request_ = CreateResourceRequestForNavigation(
      net::HttpRequestHeaders::kGetMethod, url,
      network::mojom::RequestDestination::kDocument,
      prefetch_request_->initial_referrer(), isolation_info,
      /*devtools_observer=*/mojo::NullRemote(), net::RequestPriority::HIGHEST,
      /*is_main_frame=*/true);

  auto receiver = url_loader_.InitWithNewPipeAndPassReceiver();
  auto client_remote =
      url_loader_client_receiver_.InitWithNewPipeAndPassRemote();

  // Copy `ResourceRequest` here as `PrefetchStreamingURLLoader::Start()` does.
  // Please see crbug.com/444482515 and the comment on
  // `PrefetchStreamingURLLoader::Start()` for more details.
  // TODO(crbug.com/452389538): Double-check this process is needed/reasonable
  // to maintain here.
  network::ResourceRequest new_resource_request(*resource_request_);
  if (!new_resource_request.trusted_params) {
    new_resource_request.trusted_params.emplace();
  }
  new_resource_request.trusted_params->include_request_cookies_with_response =
      true;

  mojo::Remote<network::mojom::URLLoaderFactory> factory(
      std::move(url_loader_factory));
  factory->CreateLoaderAndStart(std::move(receiver), /*request_id=*/0,
                                NavigationURLLoader::GetURLLoaderOptions(
                                    /*is_outermost_main_frame=*/true),
                                new_resource_request, std::move(client_remote),
                                net::MutableNetworkTrafficAnnotationTag(
                                    kNavigationalPrefetchTrafficAnnotation));
}

// ----------------------------------------------------------------
// Methods that can be called from any thread.
PrePrefetchContainer::~PrePrefetchContainer() = default;

}  // namespace content
