// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATA_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_DATA_URL_LOADER_FACTORY_H_

#include "base/macros.h"
#include "content/public/browser/non_network_url_loader_factory_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

// URLLoaderFactory for handling data: URLs.
class DataURLLoaderFactory : public NonNetworkURLLoaderFactoryBase {
 public:
  // Returns mojo::PendingRemote to a newly constructed DataURLLoadedFactory.
  // The factory is self-owned - it will delete itself once there are no more
  // receivers (including the receiver associated with the returned
  // mojo::PendingRemote and the receivers bound by the Clone method).
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create();

  // Helper for situations when the factory will be used only once with a |url|
  // that might be larger than the GURL serialization limit. The factory will
  // check that the passed in url to CreateLoaderAndStart either matches or is
  // empty (because it was truncated).
  static mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateForOneSpecificUrl(const GURL& url);

 private:
  // Initializes a factory with a GURL, which is useful if this factory will
  // be used only once with a GURL that can be larger than the GURL
  // serialization limit. The factory will check that the passed in url to
  // CreateLoaderAndStart either matches or is empty (because it was truncated).
  DataURLLoaderFactory(
      const GURL& url,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  // network::mojom::URLLoaderFactory:
  ~DataURLLoaderFactory() override;
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(DataURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DATA_URL_LOADER_FACTORY_H_
