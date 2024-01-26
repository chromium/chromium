// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/network_download_pending_url_loader_factory.h"

#include "components/download/public/common/download_task_runner.h"
#include "content/browser/url_loader_factory_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

NetworkDownloadPendingURLLoaderFactory::NetworkDownloadPendingURLLoaderFactory(
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    network::URLLoaderFactoryBuilder factory_builder)
    : url_loader_factory_getter_(url_loader_factory_getter),
      factory_builder_(std::move(factory_builder)) {}

NetworkDownloadPendingURLLoaderFactory::
    ~NetworkDownloadPendingURLLoaderFactory() = default;

scoped_refptr<network::SharedURLLoaderFactory>
NetworkDownloadPendingURLLoaderFactory::CreateFactory() {
  DCHECK(download::GetIOTaskRunner());
  DCHECK(download::GetIOTaskRunner()->BelongsToCurrentThread());
  if (lazy_factory_)
    return lazy_factory_;

  lazy_factory_ = std::move(factory_builder_)
                      .Finish(url_loader_factory_getter_->GetNetworkFactory());
  return lazy_factory_;
}

}  // namespace content
