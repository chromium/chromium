// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_URL_LOADER_FACTORY_PROXY_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_URL_LOADER_FACTORY_PROXY_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

const char kSecSharedStorageDataOriginHeader[] =
    "Sec-Shared-Storage-Data-Origin";

// Proxy URLLoaderFactoryFactory, to limit the requests that a shared storage
// worklet can make.
class CONTENT_EXPORT SharedStorageURLLoaderFactoryProxy
    : public network::mojom::URLLoaderFactory {
 public:
  SharedStorageURLLoaderFactoryProxy(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          frame_url_loader_factory,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
      const url::Origin& frame_origin,
      const url::Origin& data_origin,
      const GURL& script_url,
      network::mojom::CredentialsMode credentials_mode,
      const net::SiteForCookies& site_for_cookies);
  SharedStorageURLLoaderFactoryProxy(
      const SharedStorageURLLoaderFactoryProxy&) = delete;
  SharedStorageURLLoaderFactoryProxy& operator=(
      const SharedStorageURLLoaderFactoryProxy&) = delete;
  ~SharedStorageURLLoaderFactoryProxy() override;

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  mojo::Remote<network::mojom::URLLoaderFactory> frame_url_loader_factory_;

  mojo::Receiver<network::mojom::URLLoaderFactory> receiver_;

  const url::Origin frame_origin_;

  const url::Origin data_origin_;

  const GURL script_url_;

  const network::mojom::CredentialsMode credentials_mode_;

  const net::SiteForCookies site_for_cookies_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_URL_LOADER_FACTORY_PROXY_H_
