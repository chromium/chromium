// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/resource_broker/resource_broker_service_impl.h"

#include <utility>

namespace resource_broker {

ResourceBrokerServiceImpl::ResourceBrokerServiceImpl(
    mojo::PendingReceiver<mojom::ResourceBrokerService> receiver)
    : receiver_(this, std::move(receiver)) {}

ResourceBrokerServiceImpl::~ResourceBrokerServiceImpl() = default;

}  // namespace resource_broker
