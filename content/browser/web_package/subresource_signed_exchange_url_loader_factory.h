// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SUBRESOURCE_SIGNED_EXCHANGE_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SUBRESOURCE_SIGNED_EXCHANGE_URL_LOADER_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache_entry.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/origin.h"

namespace content {

// A URLLoaderFactory which handles a signed exchange subresource request from
// renderer process.
class CONTENT_EXPORT SubresourceSignedExchangeURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  SubresourceSignedExchangeURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      std::unique_ptr<const PrefetchedSignedExchangeCacheEntry> entry,
      const url::Origin& request_initiator_origin_lock);

  SubresourceSignedExchangeURLLoaderFactory(
      const SubresourceSignedExchangeURLLoaderFactory&) = delete;
  SubresourceSignedExchangeURLLoaderFactory& operator=(
      const SubresourceSignedExchangeURLLoaderFactory&) = delete;

  ~SubresourceSignedExchangeURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  void OnMojoDisconnect();

  std::unique_ptr<const PrefetchedSignedExchangeCacheEntry> entry_;
  const url::Origin request_initiator_origin_lock_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  scoped_refptr<base::RefCountedData<network::orb::PerFactoryState>>
      orb_state_ = base::MakeRefCounted<
          base::RefCountedData<network::orb::PerFactoryState>>();
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SUBRESOURCE_SIGNED_EXCHANGE_URL_LOADER_FACTORY_H_
