// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATA_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_DATA_URL_LOADER_FACTORY_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

// URLLoaderFactory for handling data: URLs.
class DataURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  DataURLLoaderFactory();

  // Initializes a factory with a GURL, which is useful if this factory will
  // be used only once with a GURL that can be larger than the GURL
  // serialization limit. The factory will check that the passed in url to
  // CreateLoaderAndStart either matches or is empty (because it was truncated).
  explicit DataURLLoaderFactory(const GURL& url);
  ~DataURLLoaderFactory() override;

 private:
  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader) override;

  GURL url_;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;

  DISALLOW_COPY_AND_ASSIGN(DataURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DATA_URL_LOADER_FACTORY_H_
