// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_WRAPPER_SHARED_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_WRAPPER_SHARED_URL_LOADER_FACTORY_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

// A PendingSharedURLLoaderFactory implementation that wraps a
// mojo::PendingRemote<network::mojom::URLLoaderFactory>.
class COMPONENT_EXPORT(NETWORK_CPP) WrapperPendingSharedURLLoaderFactory
    : public network::PendingSharedURLLoaderFactory {
 public:
  WrapperPendingSharedURLLoaderFactory();
  explicit WrapperPendingSharedURLLoaderFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote);

  ~WrapperPendingSharedURLLoaderFactory() override;

 private:
  // PendingSharedURLLoaderFactory implementation.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote_;
};

// A SharedURLLoaderFactory implementation that wraps a
// RemoteTemplateType<network::mojom::URLLoaderFactory>.
template <template <typename> class RemoteTemplateType>
class WrapperSharedURLLoaderFactoryBase
    : public network::SharedURLLoaderFactory {
 public:
  using RemoteType = RemoteTemplateType<network::mojom::URLLoaderFactory>;
  using PendingType = typename RemoteType::PendingType;

  explicit WrapperSharedURLLoaderFactoryBase(RemoteType factory_remote)
      : factory_remote_(std::move(factory_remote)) {}

  explicit WrapperSharedURLLoaderFactoryBase(PendingType pending_factory_remote)
      : factory_remote_(std::move(pending_factory_remote)) {}

  // SharedURLLoaderFactory implementation:

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    if (!factory_remote_)
      return;
    factory_remote_->CreateLoaderAndStart(std::move(loader), request_id,
                                          options, request, std::move(client),
                                          traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    if (!factory_remote_)
      return;
    factory_remote_->Clone(std::move(receiver));
  }

  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_factory_remote;
    if (factory_remote_) {
      factory_remote_->Clone(
          pending_factory_remote.InitWithNewPipeAndPassReceiver());
    }
    return std::make_unique<WrapperPendingSharedURLLoaderFactory>(
        std::move(pending_factory_remote));
  }

 private:
  ~WrapperSharedURLLoaderFactoryBase() override = default;

  RemoteType factory_remote_;
};

using WrapperSharedURLLoaderFactory =
    WrapperSharedURLLoaderFactoryBase<mojo::Remote>;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_WRAPPER_SHARED_URL_LOADER_FACTORY_H_
