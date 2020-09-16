// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/non_network_url_loader_factory_base.h"

#include <utility>

namespace content {

NonNetworkURLLoaderFactoryBase::NonNetworkURLLoaderFactoryBase(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &NonNetworkURLLoaderFactoryBase::OnDisconnect, base::Unretained(this)));
  receivers_.Add(this, std::move(factory_receiver));
}

NonNetworkURLLoaderFactoryBase::~NonNetworkURLLoaderFactoryBase() = default;

void NonNetworkURLLoaderFactoryBase::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  receivers_.Add(this, std::move(loader));
}

void NonNetworkURLLoaderFactoryBase::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (receivers_.empty())
    delete this;
}

}  // namespace content
