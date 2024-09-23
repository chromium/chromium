// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NOT_IMPLEMENTED_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NOT_IMPLEMENTED_URL_LOADER_FACTORY_H_

#include "base/component_export.h"
#include "base/location.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

// A URLLoaderFactory which just fails to create a loader with
// net::ERR_NOT_IMPLEMENTED.
class COMPONENT_EXPORT(NETWORK_CPP) NotImplementedURLLoaderFactory final
    : public SelfDeletingURLLoaderFactory {
 public:
  // Returns mojo::PendingRemote to a newly constructed
  // NotImplementedURLLoaderFactory.  The factory is self-owned - it will delete
  // itself once there are no more receivers (including the receiver associated
  // with the returned mojo::PendingRemote and the receivers bound by the Clone
  // method).
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      base::Location creator_location = base::Location::Current());

  NotImplementedURLLoaderFactory(const NotImplementedURLLoaderFactory&) =
      delete;
  NotImplementedURLLoaderFactory& operator=(
      const NotImplementedURLLoaderFactory&) = delete;

  ~NotImplementedURLLoaderFactory() override;

 private:
  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  // Constructs a NotImplementedURLLoaderFactory object that will self-delete
  // once all receivers disconnect (including |factory_receiver| below as well
  // as receivers that connect via the Clone method).
  NotImplementedURLLoaderFactory(
      base::Location creator_location,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  base::Location creator_location_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NOT_IMPLEMENTED_URL_LOADER_FACTORY_H_
