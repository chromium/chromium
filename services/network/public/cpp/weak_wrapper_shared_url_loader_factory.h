// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_WEAK_WRAPPER_SHARED_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_WEAK_WRAPPER_SHARED_URL_LOADER_FACTORY_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

namespace network {

// A SharedURLLoaderFactory implementation that wraps a raw
// mojom::URLLoaderFactory pointer.
class COMPONENT_EXPORT(NETWORK_CPP) WeakWrapperSharedURLLoaderFactory
    : public network::SharedURLLoaderFactory {
 public:
  // Warning: since this will not own `factory_ptr`, and is ref-counted (thus
  // potentially having distant things extend its lifetime), it's easy to end up
  // with a dangling pointer. Make sure to call `Detach()` when the lifetime
  // of the underlying implementation is about to end.
  //
  // If you're using this with TestURLLoaderFactory, consider using its
  // GetSafeWeakWrapper() which will Detach() automatically.
  explicit WeakWrapperSharedURLLoaderFactory(
      mojom::URLLoaderFactory* factory_ptr);

  // A lazy variant. This is useful when transitionning code that sets up
  // heavy-weight infrastructure, injects a shared_ptr<SharedURLLoaderFactory>
  // into lots of places, but doesn't actually use it.
  explicit WeakWrapperSharedURLLoaderFactory(
      base::OnceCallback<mojom::URLLoaderFactory*()> make_factory_ptr);

  // Detaches from the raw mojom::URLLoaderFactory pointer. All subsequent calls
  // to CreateLoaderAndStart() will fail silently.
  void Detach();

  // SharedURLLoaderFactory implementation.
  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> loader,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            mojo::PendingRemote<mojom::URLLoaderClient> client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override;
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

 private:
  ~WeakWrapperSharedURLLoaderFactory() override;

  // Uses whichever of make_factory_ptr_ or factory_ptr_ is relevant.
  mojom::URLLoaderFactory* factory();

  base::OnceCallback<mojom::URLLoaderFactory*()> make_factory_ptr_;

  // Not owned.
  raw_ptr<mojom::URLLoaderFactory> factory_ptr_ = nullptr;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_WEAK_WRAPPER_SHARED_URL_LOADER_FACTORY_H_
