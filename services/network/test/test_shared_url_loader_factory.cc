// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_shared_url_loader_factory.h"

#include "base/notreached.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace network {

TestSharedURLLoaderFactory::TestSharedURLLoaderFactory(
    NetworkService* network_service,
    bool is_trusted) {
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  url_request_context_ = context_builder->Build();
  mojo::Remote<mojom::NetworkContext> network_context;
  network_context_ = std::make_unique<NetworkContext>(
      network_service, network_context.BindNewPipeAndPassReceiver(),
      url_request_context_.get(),
      /*cors_exempt_header_list=*/std::vector<std::string>());
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_orb_enabled = false;
  params->is_trusted = is_trusted;
  network_context_->CreateURLLoaderFactory(
      url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));
}

TestSharedURLLoaderFactory::~TestSharedURLLoaderFactory() {}

void TestSharedURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  num_created_loaders_++;
  url_loader_factory_->CreateLoaderAndStart(
      std::move(loader), request_id, options, std::move(request),
      std::move(client), traffic_annotation);
}

void TestSharedURLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
  url_loader_factory_->Clone(std::move(receiver));
}

mojom::NetworkContext* TestSharedURLLoaderFactory::network_context() {
  return network_context_.get();
}

// PendingSharedURLLoaderFactory implementation
std::unique_ptr<PendingSharedURLLoaderFactory>
TestSharedURLLoaderFactory::Clone() {
  return std::make_unique<CrossThreadPendingSharedURLLoaderFactory>(this);
}

}  // namespace network
