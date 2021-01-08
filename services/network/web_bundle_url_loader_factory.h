// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEB_BUNDLE_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_WEB_BUNDLE_URL_LOADER_FACTORY_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) WebBundleURLLoaderFactory
    : public mojom::URLLoaderFactory {
 public:
  WebBundleURLLoaderFactory(
      const GURL& bundle_url,
      mojo::Remote<mojom::WebBundleHandle> web_bundle_handle,
      const base::Optional<url::Origin>& request_initiator_origin_lock);
  ~WebBundleURLLoaderFactory() override;
  WebBundleURLLoaderFactory(const WebBundleURLLoaderFactory&) = delete;
  WebBundleURLLoaderFactory& operator=(const WebBundleURLLoaderFactory&) =
      delete;

  base::WeakPtr<WebBundleURLLoaderFactory> GetWeakPtr() const;

  void SetBundleStream(mojo::ScopedDataPipeConsumerHandle body);
  mojo::PendingRemote<mojom::URLLoaderClient> WrapURLLoaderClient(
      mojo::PendingRemote<mojom::URLLoaderClient> wrapped);

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> receiver,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& url_request,
                            mojo::PendingRemote<mojom::URLLoaderClient> client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;

  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override;

 private:
  class BundleDataSource;
  class URLLoader;

  void StartLoad(URLLoader* loader);
  void OnMetadataParsed(web_package::mojom::BundleMetadataPtr metadata,
                        web_package::mojom::BundleMetadataParseErrorPtr error);
  void OnResponseParsed(base::WeakPtr<URLLoader> loader,
                        web_package::mojom::BundleResponsePtr response,
                        web_package::mojom::BundleResponseParseErrorPtr error);

  GURL bundle_url_;
  mojo::Remote<mojom::WebBundleHandle> web_bundle_handle_;
  const base::Optional<::url::Origin> request_initiator_origin_lock_;
  std::unique_ptr<BundleDataSource> source_;
  mojo::Remote<web_package::mojom::WebBundleParser> parser_;
  web_package::mojom::BundleMetadataPtr metadata_;
  web_package::mojom::BundleMetadataParseErrorPtr metadata_error_;
  std::vector<base::WeakPtr<URLLoader>> pending_loaders_;

  base::WeakPtrFactory<WebBundleURLLoaderFactory> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_BUNDLE_URL_LOADER_FACTORY_H_
