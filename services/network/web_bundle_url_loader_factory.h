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

class WebBundleMemoryQuotaConsumer;

class COMPONENT_EXPORT(NETWORK_SERVICE) WebBundleURLLoaderFactory {
 public:
  // Used for UMA. Append-only.
  enum class SubresourceWebBundleLoadResult {
    kSuccess = 0,
    kMetadataParseError = 1,
    kMemoryQuotaExceeded = 2,
    kMaxValue = kMemoryQuotaExceeded,
  };

  WebBundleURLLoaderFactory(
      const GURL& bundle_url,
      mojo::Remote<mojom::WebBundleHandle> web_bundle_handle,
      const base::Optional<url::Origin>& request_initiator_origin_lock,
      std::unique_ptr<WebBundleMemoryQuotaConsumer>
          web_bundle_memory_quota_consumer);
  ~WebBundleURLLoaderFactory();
  WebBundleURLLoaderFactory(const WebBundleURLLoaderFactory&) = delete;
  WebBundleURLLoaderFactory& operator=(const WebBundleURLLoaderFactory&) =
      delete;

  base::WeakPtr<WebBundleURLLoaderFactory> GetWeakPtr() const;

  void SetBundleStream(mojo::ScopedDataPipeConsumerHandle body);
  void ReportErrorAndCancelPendingLoaders(SubresourceWebBundleLoadResult result,
                                          mojom::WebBundleErrorType error,
                                          const std::string& message);
  mojo::PendingRemote<mojom::URLLoaderClient> WrapURLLoaderClient(
      mojo::PendingRemote<mojom::URLLoaderClient> wrapped);

  void StartSubresourceRequest(
      mojo::PendingReceiver<mojom::URLLoader> receiver,
      const ResourceRequest& url_request,
      mojo::PendingRemote<mojom::URLLoaderClient> client);

 private:
  class BundleDataSource;
  class URLLoader;

  bool HasError() const;
  void StartLoad(URLLoader* loader);
  void OnMetadataParsed(web_package::mojom::BundleMetadataPtr metadata,
                        web_package::mojom::BundleMetadataParseErrorPtr error);
  void OnResponseParsed(base::WeakPtr<URLLoader> loader,
                        web_package::mojom::BundleResponsePtr response,
                        web_package::mojom::BundleResponseParseErrorPtr error);
  void OnMemoryQuotaExceeded();
  void OnDataCompleted();
  void MaybeRecordLoadResult(SubresourceWebBundleLoadResult result);

  GURL bundle_url_;
  mojo::Remote<mojom::WebBundleHandle> web_bundle_handle_;
  const base::Optional<::url::Origin> request_initiator_origin_lock_;
  std::unique_ptr<WebBundleMemoryQuotaConsumer>
      web_bundle_memory_quota_consumer_;
  std::unique_ptr<BundleDataSource> source_;
  mojo::Remote<web_package::mojom::WebBundleParser> parser_;
  web_package::mojom::BundleMetadataPtr metadata_;
  base::Optional<SubresourceWebBundleLoadResult> load_result_;
  bool data_completed_ = false;
  std::vector<base::WeakPtr<URLLoader>> pending_loaders_;

  base::WeakPtrFactory<WebBundleURLLoaderFactory> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_BUNDLE_URL_LOADER_FACTORY_H_
