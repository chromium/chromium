// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_RESOURCE_BROKER_RESOURCE_BROKER_SERVICE_IMPL_H_
#define CONTENT_SERVICES_RESOURCE_BROKER_RESOURCE_BROKER_SERVICE_IMPL_H_

#include "content/services/resource_broker/public/mojom/resource_broker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace resource_broker {

class ResourceBrokerServiceImpl : public mojom::ResourceBrokerService {
 public:
  explicit ResourceBrokerServiceImpl(
      mojo::PendingReceiver<mojom::ResourceBrokerService> receiver);

  ResourceBrokerServiceImpl(const ResourceBrokerServiceImpl&) = delete;
  ResourceBrokerServiceImpl& operator=(const ResourceBrokerServiceImpl&) =
      delete;

  ~ResourceBrokerServiceImpl() override;

 private:
  mojo::Receiver<mojom::ResourceBrokerService> receiver_;
};

}  // namespace resource_broker

#endif  // CONTENT_SERVICES_RESOURCE_BROKER_RESOURCE_BROKER_SERVICE_IMPL_H_
