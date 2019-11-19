// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/navigable_contents_factory_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "services/content/navigable_contents_impl.h"
#include "services/content/service.h"

namespace content {

NavigableContentsFactoryImpl::NavigableContentsFactoryImpl(
    Service* service,
    mojo::PendingReceiver<mojom::NavigableContentsFactory> receiver)
    : service_(service), receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&Service::RemoveNavigableContentsFactory,
                     base::Unretained(service_), this));
}

NavigableContentsFactoryImpl::~NavigableContentsFactoryImpl() = default;

void NavigableContentsFactoryImpl::CreateContents(
    mojom::NavigableContentsParamsPtr params,
    mojo::PendingReceiver<mojom::NavigableContents> receiver,
    mojo::PendingRemote<mojom::NavigableContentsClient> client) {
  service_->AddNavigableContents(std::make_unique<NavigableContentsImpl>(
      service_, std::move(params), std::move(receiver), std::move(client)));
}

}  // namespace content
