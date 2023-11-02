// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"

namespace network {

WrapperPendingSharedURLLoaderFactory::WrapperPendingSharedURLLoaderFactory() =
    default;

WrapperPendingSharedURLLoaderFactory::WrapperPendingSharedURLLoaderFactory(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote)
    : factory_remote_(std::move(factory_remote)) {}

WrapperPendingSharedURLLoaderFactory::~WrapperPendingSharedURLLoaderFactory() =
    default;

scoped_refptr<network::SharedURLLoaderFactory>
WrapperPendingSharedURLLoaderFactory::CreateFactory() {
  return base::MakeRefCounted<WrapperSharedURLLoaderFactory>(
      std::move(factory_remote_));
}

}  // namespace network
