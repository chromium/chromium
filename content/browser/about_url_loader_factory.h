// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ABOUT_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_ABOUT_URL_LOADER_FACTORY_H_

#include "base/macros.h"
#include "content/public/browser/non_network_url_loader_factory_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

// URLLoaderFactory for handling about: URLs. This treats everything as
// about:blank since no other about: features should be available to web
// content.
class AboutURLLoaderFactory : public NonNetworkURLLoaderFactoryBase {
 public:
  // Returns mojo::PendingRemote to a newly constructed AboutURLLoadedFactory.
  // The factory is self-owned - it will delete itself once there are no more
  // receivers (including the receiver associated with the returned
  // mojo::PendingRemote and the receivers bound by the Clone method).
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create();

 private:
  explicit AboutURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  // network::mojom::URLLoaderFactory:
  ~AboutURLLoaderFactory() override;
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  DISALLOW_COPY_AND_ASSIGN(AboutURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ABOUT_URL_LOADER_FACTORY_H_
