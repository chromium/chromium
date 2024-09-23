// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/not_implemented_url_loader_factory.h"

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {

NotImplementedURLLoaderFactory::NotImplementedURLLoaderFactory(
    base::Location creator_location,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
    : SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
      creator_location_(creator_location) {}

NotImplementedURLLoaderFactory::~NotImplementedURLLoaderFactory() = default;

void NotImplementedURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  SCOPED_CRASH_KEY_STRING256("bug40216482", "NoImplFactoryCreator",
                             creator_location_.ToString());
  DUMP_WILL_BE_NOTREACHED();
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_NOT_IMPLEMENTED;
  mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
      ->OnComplete(status);
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
NotImplementedURLLoaderFactory::Create(base::Location creator_location) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

  // The NotImplementedURLLoaderFactory will delete itself when there are no
  // more receivers - see the NotImplementedURLLoaderFactory::OnDisconnect
  // method.
  new NotImplementedURLLoaderFactory(
      creator_location, pending_remote.InitWithNewPipeAndPassReceiver());

  return pending_remote;
}

}  // namespace network
