// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/not_implemented_url_loader_factory.h"

#include "base/logging.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {

NotImplementedURLLoaderFactory::NotImplementedURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver) {
  // TODO(lukasza): Reuse content::NonNetworkURLLoaderFactoryBase (after moving
  // it to //services/network/public/cpp and maybe renaming it to
  // SelfDeletingURLLoaderFactory.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &NotImplementedURLLoaderFactory::OnDisconnect, base::Unretained(this)));
  receivers_.Add(this, std::move(factory_receiver));
}

NotImplementedURLLoaderFactory::~NotImplementedURLLoaderFactory() = default;

void NotImplementedURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  NOTREACHED();
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_NOT_IMPLEMENTED;
  mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
      ->OnComplete(status);
}

void NotImplementedURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void NotImplementedURLLoaderFactory::OnDisconnect() {
  if (receivers_.empty()) {
    // If there are no more |receivers_|, then no instance methods of |this| can
    // be called in the future (mojo methods Clone and CreateLoaderAndStart
    // should be the only public entrypoints).  Therefore, it is safe to delete
    // |this| at this point.
    delete this;
  }
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
NotImplementedURLLoaderFactory::Create() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

  // The NotImplementedURLLoaderFactory will delete itself when there are no
  // more receivers - see the NotImplementedURLLoaderFactory::OnDisconnect
  // method.
  new NotImplementedURLLoaderFactory(
      pending_remote.InitWithNewPipeAndPassReceiver());

  return pending_remote;
}

}  // namespace network
