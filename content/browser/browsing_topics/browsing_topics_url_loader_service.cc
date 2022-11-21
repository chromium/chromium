// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_url_loader_service.h"

#include "base/bind.h"
#include "content/browser/browsing_topics/browsing_topics_url_loader.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace content {

BrowsingTopicsURLLoaderService::BindContext::BindContext(
    scoped_refptr<network::SharedURLLoaderFactory> factory)
    : factory(factory) {}

BrowsingTopicsURLLoaderService::BindContext::BindContext(
    const std::unique_ptr<BindContext>& other)
    : document(other->document), factory(other->factory) {}

void BrowsingTopicsURLLoaderService::BindContext::OnDidCommitNavigation(
    WeakDocumentPtr committed_document) {
  document = committed_document;
}

BrowsingTopicsURLLoaderService::BindContext::~BindContext() = default;

BrowsingTopicsURLLoaderService::BrowsingTopicsURLLoaderService() = default;

BrowsingTopicsURLLoaderService::~BrowsingTopicsURLLoaderService() = default;

base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext>
BrowsingTopicsURLLoaderService::GetFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto factory_bundle =
      network::SharedURLLoaderFactory::Create(std::move(pending_factory));

  auto bind_context = std::make_unique<BindContext>(factory_bundle);

  base::WeakPtr<BindContext> weak_bind_context =
      bind_context->weak_ptr_factory.GetWeakPtr();

  loader_factory_receivers_.Add(this, std::move(receiver),
                                std::move(bind_context));

  return weak_bind_context;
}

void BrowsingTopicsURLLoaderService::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const std::unique_ptr<BindContext>& current_context =
      loader_factory_receivers_.current_context();

  auto loader = std::make_unique<BrowsingTopicsURLLoader>(
      current_context->document, request_id, options, resource_request,
      std::move(client), traffic_annotation, current_context->factory);

  auto* raw_loader = loader.get();

  loader_receivers_.Add(raw_loader, std::move(receiver), std::move(loader));
}

void BrowsingTopicsURLLoaderService::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  loader_factory_receivers_.Add(
      this, std::move(receiver),
      std::make_unique<BindContext>(
          loader_factory_receivers_.current_context()));
}

}  // namespace content
