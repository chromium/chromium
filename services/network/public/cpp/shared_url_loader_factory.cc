// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {

// static
scoped_refptr<SharedURLLoaderFactory> SharedURLLoaderFactory::Create(
    std::unique_ptr<PendingSharedURLLoaderFactory> pending_factory) {
  DCHECK(pending_factory);
  return pending_factory->CreateFactory();
}

SharedURLLoaderFactory::~SharedURLLoaderFactory() = default;

PendingSharedURLLoaderFactory::PendingSharedURLLoaderFactory() = default;

PendingSharedURLLoaderFactory::~PendingSharedURLLoaderFactory() = default;

bool SharedURLLoaderFactory::BypassRedirectChecks() const {
  return false;
}

}  // namespace network
