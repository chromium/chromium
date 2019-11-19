// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_URL_LOADER_FACTORY_INFO_H_
#define CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_URL_LOADER_FACTORY_INFO_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class URLLoaderFactoryGetter;

// Wrapper of a URLLoaderFactoryGetter that can be passed to another thread
// to retrieve URLLoaderFactory.
class NetworkDownloadURLLoaderFactoryInfo
    : public network::SharedURLLoaderFactoryInfo {
 public:
  NetworkDownloadURLLoaderFactoryInfo(
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          proxy_factory_remote,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          proxy_factory_receiver);
  ~NetworkDownloadURLLoaderFactoryInfo() override;

 protected:
  // SharedURLLoaderFactoryInfo implementation.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

 private:
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> proxy_factory_remote_;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      proxy_factory_receiver_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDownloadURLLoaderFactoryInfo);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_URL_LOADER_FACTORY_INFO_H_
