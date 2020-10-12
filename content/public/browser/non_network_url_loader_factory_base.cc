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

void NonNetworkURLLoaderFactoryBase::DisconnectReceiversAndDestroy() {
  // Clear |receivers_| to explicitly make sure that no further method
  // invocations or disconnection notifications will happen.  (per the
  // comment of mojo::ReceiverSet::Clear)
  receivers_.Clear();

  // Similarly to OnDisconnect, if there are no more |receivers_|, then no
  // instance methods of |this| can be called in the future (mojo methods Clone
  // and CreateLoaderAndStart should be the only public entrypoints).
  // Therefore, it is safe to delete |this| at this point.
  delete this;
}

void NonNetworkURLLoaderFactoryBase::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  receivers_.Add(this, std::move(loader));
}

void NonNetworkURLLoaderFactoryBase::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (receivers_.empty()) {
    // If there are no more |receivers_|, then no instance methods of |this| can
    // be called in the future (mojo methods Clone and CreateLoaderAndStart
    // should be the only public entrypoints).  Therefore, it is safe to delete
    // |this| at this point.
    delete this;
  }
}

}  // namespace content
