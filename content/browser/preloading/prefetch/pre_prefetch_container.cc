// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/pre_prefetch_container.h"

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_resource_request_utils.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"
#include "url/gurl.h"

namespace content {

// ------------------------------------------------------------------------
// Methods to be called from the `PrePrefetchServiceCore` sequence.

// static
std::unique_ptr<PrePrefetchContainer> PrePrefetchContainer::CreateAndStart(
    base::PassKey<PrePrefetchServiceImpl>,
    std::unique_ptr<const PrefetchRequest> prefetch_request,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    const PrefetchUpdateHeadersParams& ui_thread_pre_calculated_headers,
    const std::vector<PrePrefetchUpdateHeadersCallback>&
        non_ui_thread_update_headers_callbacks) {
  // This should only be called from `PrePrefetchServiceCore` sequence through
  // `PrePrefetchServiceImpl`.
  return CreateAndStartInternal(
      std::move(prefetch_request), std::move(url_loader_factory),
      ui_thread_pre_calculated_headers, non_ui_thread_update_headers_callbacks);
}

// static
std::unique_ptr<PrePrefetchContainer>
PrePrefetchContainer::CreateAndStartForTesting(  // IN-TEST
    std::unique_ptr<const PrefetchRequest> prefetch_request,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    const PrefetchUpdateHeadersParams& ui_thread_pre_calculated_headers,
    const std::vector<PrePrefetchUpdateHeadersCallback>&
        non_ui_thread_update_headers_callbacks) {
  return CreateAndStartInternal(
      std::move(prefetch_request), std::move(url_loader_factory),
      ui_thread_pre_calculated_headers, non_ui_thread_update_headers_callbacks);
}

// static
std::unique_ptr<PrePrefetchContainer>
PrePrefetchContainer::CreateAndStartInternal(
    std::unique_ptr<const PrefetchRequest> prefetch_request,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    const PrefetchUpdateHeadersParams& ui_thread_pre_calculated_headers,
    const std::vector<PrePrefetchUpdateHeadersCallback>&
        non_ui_thread_update_headers_callbacks) {
  auto container = std::make_unique<PrePrefetchContainer>(
      base::PassKey<PrePrefetchContainer>(), std::move(prefetch_request));
  container->Start(std::move(url_loader_factory),
                   ui_thread_pre_calculated_headers,
                   non_ui_thread_update_headers_callbacks);
  return container;
}

PrePrefetchContainer::PrePrefetchContainer(
    base::PassKey<PrePrefetchContainer>,
    std::unique_ptr<const PrefetchRequest> prefetch_request)
    : prefetch_request_(std::move(prefetch_request)) {
  // This should only be called from `PrePrefetchServiceCore` sequence
  // through `PrePrefetchServiceImpl`, or the sequence used on the test.
  CHECK(!BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
}

void PrePrefetchContainer::Start(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    const PrefetchUpdateHeadersParams& ui_thread_pre_calculated_headers,
    const std::vector<PrePrefetchUpdateHeadersCallback>&
        non_ui_thread_update_headers_callbacks) {
  TRACE_EVENT("loading", "PrePrefetchContainer::Start", "url",
              prefetch_request_->key().url());

  CHECK(!url_loader_);
  CHECK(!url_loader_client_receiver_);

  if (!url_loader_factory) {
    return;
  }

  // TODO(crbug.com/452389538): Perform prefetch eligibility checks that are
  // required before starting PrePrefetch's network request.

  // Construct a proper `ResourceRequest`, using the pre-calculated headers
  // on the UI thread.
  resource_request_ = MakeInitialResourceRequestForPrePrefetch(
      *prefetch_request_, ui_thread_pre_calculated_headers);

  // Apply any custom headers provided by the service via the thread-safe
  // callbacks after the initial `ResourceRequest` construction.
  for (const auto& callback : non_ui_thread_update_headers_callbacks) {
    PrefetchUpdateHeadersParams params = callback.Run(*resource_request_);

    for (const auto& removed_header : params.removed_headers) {
      resource_request_->headers.RemoveHeader(removed_header);
      resource_request_->cors_exempt_headers.RemoveHeader(removed_header);
    }
    resource_request_->headers.MergeFrom(params.modified_headers);
    resource_request_->cors_exempt_headers.MergeFrom(
        params.modified_cors_exempt_headers);
  }

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
// Methods should be called from the UI thread.

std::unique_ptr<const PrefetchRequest>
PrePrefetchContainer::TakePrefetchRequestOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(prefetch_request_);
  return std::move(prefetch_request_);
}

std::unique_ptr<network::ResourceRequest>
PrePrefetchContainer::TakeResourceRequestOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(resource_request_);
  return std::move(resource_request_);
}

mojo::PendingRemote<network::mojom::URLLoader>
PrePrefetchContainer::TakePendingURLLoaderOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(url_loader_);
  return std::move(url_loader_);
}

mojo::PendingReceiver<network::mojom::URLLoaderClient>
PrePrefetchContainer::TakePendingURLLoaderClientReceiverOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(url_loader_client_receiver_);
  return std::move(url_loader_client_receiver_);
}

// ----------------------------------------------------------------
// Methods that can be called from any thread.
PrePrefetchContainer::~PrePrefetchContainer() = default;

}  // namespace content
