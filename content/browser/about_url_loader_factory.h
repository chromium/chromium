// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ABOUT_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_ABOUT_URL_LOADER_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"

namespace content {

// URLLoaderFactory for handling about: URLs. This treats everything as
// about:blank since no other about: features should be available to web
// content.
class AboutURLLoaderFactory : public network::SelfDeletingURLLoaderFactory {
 public:
  // Returns mojo::PendingRemote to a newly constructed AboutURLLoadedFactory.
  // The factory is self-owned - it will delete itself once there are no more
  // receivers (including the receiver associated with the returned
  // mojo::PendingRemote and the receivers bound by the Clone method).
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create();

  AboutURLLoaderFactory(const AboutURLLoaderFactory&) = delete;
  AboutURLLoaderFactory& operator=(const AboutURLLoaderFactory&) = delete;

 private:
  explicit AboutURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  // network::mojom::URLLoaderFactory:
  ~AboutURLLoaderFactory() override;
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ABOUT_URL_LOADER_FACTORY_H_
