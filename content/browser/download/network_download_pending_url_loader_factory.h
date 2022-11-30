// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_PENDING_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_PENDING_URL_LOADER_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class URLLoaderFactoryGetter;

// Wrapper of a URLLoaderFactoryGetter that can be passed to another thread
// to retrieve URLLoaderFactory.
class NetworkDownloadPendingURLLoaderFactory
    : public network::PendingSharedURLLoaderFactory {
 public:
  NetworkDownloadPendingURLLoaderFactory(
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          proxy_factory_remote,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          proxy_factory_receiver);

  NetworkDownloadPendingURLLoaderFactory(
      const NetworkDownloadPendingURLLoaderFactory&) = delete;
  NetworkDownloadPendingURLLoaderFactory& operator=(
      const NetworkDownloadPendingURLLoaderFactory&) = delete;

  ~NetworkDownloadPendingURLLoaderFactory() override;

 protected:
  // PendingSharedURLLoaderFactory implementation.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

 private:
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> proxy_factory_remote_;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      proxy_factory_receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_PENDING_URL_LOADER_FACTORY_H_
