// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/identity_service.h"

#include <memory>
#include <utility>

#include "services/identity/identity_accessor_impl.h"

namespace identity {

IdentityService::IdentityService(
    signin::IdentityManager* identity_manager,
    mojo::PendingReceiver<mojom::IdentityService> receiver)
    : receiver_(this, std::move(receiver)),
      identity_manager_(identity_manager) {}

IdentityService::~IdentityService() {
  ShutDown();
}

void IdentityService::BindIdentityAccessor(
    mojo::PendingReceiver<mojom::IdentityAccessor> receiver) {
  // This instance cannot service IdentityAccessor requests once it has been
  // shut down.
  if (IsShutDown())
    return;

  identity_accessors_.Add(
      std::make_unique<IdentityAccessorImpl>(identity_manager_),
      std::move(receiver));
}

void IdentityService::ShutDown() {
  if (IsShutDown())
    return;

  identity_manager_ = nullptr;
  identity_accessors_.Clear();
}

bool IdentityService::IsShutDown() {
  return (identity_manager_ == nullptr);
}

}  // namespace identity
