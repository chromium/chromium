// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_PENDING_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_PENDING_URL_LOADER_FACTORY_H_

#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
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
      network::URLLoaderFactoryBuilder factory_builder);

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
  network::URLLoaderFactoryBuilder factory_builder_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_PENDING_URL_LOADER_FACTORY_H_
