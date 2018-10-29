// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader_service.h"

#include "base/feature_list.h"
#include "base/time/default_tick_clock.h"
#include "content/browser/loader/prefetch_url_loader.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader_throttle.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"

namespace content {

struct PrefetchURLLoaderService::BindContext {
  BindContext(int frame_tree_node_id,
              scoped_refptr<network::SharedURLLoaderFactory> factory)
      : frame_tree_node_id(frame_tree_node_id), factory(factory) {}

  explicit BindContext(const std::unique_ptr<BindContext>& other)
      : frame_tree_node_id(other->frame_tree_node_id),
        factory(other->factory) {}

  ~BindContext() = default;

  const int frame_tree_node_id;
  scoped_refptr<network::SharedURLLoaderFactory> factory;
};

PrefetchURLLoaderService::PrefetchURLLoaderService()
    : signed_exchange_prefetch_metric_recorder_(
          base::MakeRefCounted<SignedExchangePrefetchMetricRecorder>(
              base::DefaultTickClock::GetInstance())) {}

void PrefetchURLLoaderService::InitializeResourceContext(
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!resource_context_);
  DCHECK(!request_context_getter_);
  resource_context_ = resource_context;
  request_context_getter_ = request_context_getter;
}

void PrefetchURLLoaderService::GetFactory(
    network::mojom::URLLoaderFactoryRequest request,
    int frame_tree_node_id,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> factories) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto factory_bundle =
      network::SharedURLLoaderFactory::Create(std::move(factories));
  loader_factory_bindings_.AddBinding(
      this, std::move(request),
      std::make_unique<BindContext>(frame_tree_node_id, factory_bundle));
}

void PrefetchURLLoaderService::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(RESOURCE_TYPE_PREFETCH, resource_request.resource_type);
  DCHECK(resource_context_);

  if (prefetch_load_callback_for_testing_)
    prefetch_load_callback_for_testing_.Run();

  // For now we strongly bind the loader to the request, while we can
  // also possibly make the new loader owned by the factory so that
  // they can live longer than the client (i.e. run in detached mode).
  // TODO(kinuko): Revisit this.
  mojo::MakeStrongBinding(
      std::make_unique<PrefetchURLLoader>(
          routing_id, request_id, options, frame_tree_node_id_getter,
          resource_request, std::move(client), traffic_annotation,
          std::move(network_loader_factory),
          base::BindRepeating(
              &PrefetchURLLoaderService::CreateURLLoaderThrottles, this,
              resource_request, frame_tree_node_id_getter),
          resource_context_, request_context_getter_,
          signed_exchange_prefetch_metric_recorder_),
      std::move(request));
}

PrefetchURLLoaderService::~PrefetchURLLoaderService() = default;

void PrefetchURLLoaderService::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService) ||
         base::FeatureList::IsEnabled(
             blink::features::kServiceWorkerServicification));
  const auto& dispatch_context = *loader_factory_bindings_.dispatch_context();
  int frame_tree_node_id = dispatch_context.frame_tree_node_id;
  CreateLoaderAndStart(
      std::move(request), routing_id, request_id, options, resource_request,
      std::move(client), traffic_annotation, dispatch_context.factory,
      base::BindRepeating([](int id) { return id; }, frame_tree_node_id));
}

void PrefetchURLLoaderService::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  loader_factory_bindings_.AddBinding(
      this, std::move(request),
      std::make_unique<BindContext>(
          loader_factory_bindings_.dispatch_context()));
}

std::vector<std::unique_ptr<content::URLLoaderThrottle>>
PrefetchURLLoaderService::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      !request_context_getter_ ||
      !request_context_getter_->GetURLRequestContext())
    return std::vector<std::unique_ptr<content::URLLoaderThrottle>>();
  int frame_tree_node_id = frame_tree_node_id_getter.Run();
  return GetContentClient()->browser()->CreateURLLoaderThrottles(
      request, resource_context_,
      base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                          frame_tree_node_id),
      nullptr /* navigation_ui_data */, frame_tree_node_id);
}

}  // namespace content
