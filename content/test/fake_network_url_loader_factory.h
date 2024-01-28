// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_NETWORK_URL_LOADER_FACTORY_H_
#define CONTENT_TEST_FAKE_NETWORK_URL_LOADER_FACTORY_H_

#include "content/test/fake_network.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

// A URLLoaderFactory backed by FakeNetwork, see the documentation there
// for its behavior.
class FakeNetworkURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  FakeNetworkURLLoaderFactory();

  // If this constructor is used, the provided response is used for any url
  // request.
  FakeNetworkURLLoaderFactory(const std::string& headers,
                              const std::string& body,
                              bool network_accessed,
                              net::Error error_code);

  FakeNetworkURLLoaderFactory(const FakeNetworkURLLoaderFactory&) = delete;
  FakeNetworkURLLoaderFactory& operator=(const FakeNetworkURLLoaderFactory&) =
      delete;

  ~FakeNetworkURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory implementation.
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
  FakeNetwork fake_network_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_NETWORK_URL_LOADER_FACTORY_H_
