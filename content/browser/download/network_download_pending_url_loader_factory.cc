// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/network_download_pending_url_loader_factory.h"

#include "components/download/public/common/download_task_runner.h"
#include "content/browser/url_loader_factory_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"

namespace content {

NetworkDownloadPendingURLLoaderFactory::NetworkDownloadPendingURLLoaderFactory(
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> proxy_factory_remote,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        proxy_factory_receiver)
    : url_loader_factory_getter_(url_loader_factory_getter),
      proxy_factory_remote_(std::move(proxy_factory_remote)),
      proxy_factory_receiver_(std::move(proxy_factory_receiver)) {}

NetworkDownloadPendingURLLoaderFactory::
    ~NetworkDownloadPendingURLLoaderFactory() = default;

scoped_refptr<network::SharedURLLoaderFactory>
NetworkDownloadPendingURLLoaderFactory::CreateFactory() {
  DCHECK(download::GetIOTaskRunner());
  DCHECK(download::GetIOTaskRunner()->BelongsToCurrentThread());
  if (lazy_factory_)
    return lazy_factory_;
  if (proxy_factory_receiver_.is_valid()) {
    url_loader_factory_getter_->CloneNetworkFactory(
        std::move(proxy_factory_receiver_));
    lazy_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(proxy_factory_remote_));
  } else {
    lazy_factory_ = url_loader_factory_getter_->GetNetworkFactory();
  }
  return lazy_factory_;
}

}  // namespace content
