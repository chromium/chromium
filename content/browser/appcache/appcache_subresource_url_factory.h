// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_SUBRESOURCE_URL_FACTORY_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_SUBRESOURCE_URL_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

class AppCacheHost;
class AppCacheJob;
class AppCacheRequestHandler;
class AppCacheServiceImpl;

// Implements the URLLoaderFactory mojom for AppCache subresource requests.
class CONTENT_EXPORT AppCacheSubresourceURLFactory
    : public network::mojom::URLLoaderFactory {
 public:
  ~AppCacheSubresourceURLFactory() override;

  // Factory function to create an instance of the factory.
  // The |host| parameter contains the appcache host instance. This is used
  // to create the AppCacheRequestHandler instances for handling subresource
  // requests.
  static void CreateURLLoaderFactory(
      base::WeakPtr<AppCacheHost> host,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* loader_factory);

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  base::WeakPtr<AppCacheSubresourceURLFactory> GetWeakPtr();

 private:
  friend class AppCacheNetworkServiceBrowserTest;

  // TODO(michaeln): Declare SubresourceLoader here and add unittests.

  AppCacheSubresourceURLFactory(
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      base::WeakPtr<AppCacheHost> host);
  void OnMojoDisconnect();

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;
  base::WeakPtr<AppCacheHost> appcache_host_;
  base::WeakPtrFactory<AppCacheSubresourceURLFactory> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AppCacheSubresourceURLFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_FACTORY_H_
