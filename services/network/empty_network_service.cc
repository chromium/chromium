// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/empty_network_service.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace network {
namespace {

class EmptyNetworkService : public network::mojom::EmptyNetworkService {
 public:
  explicit EmptyNetworkService(
      mojo::PendingReceiver<network::mojom::EmptyNetworkService> receiver) {
    receiver_.Bind(std::move(receiver));
  }

 private:
  // network::mojom::EmptyNetworkService implementation:
  void Ping(int32_t value, PingCallback callback) override {
    std::move(callback).Run(value);
  }
  mojo::Receiver<network::mojom::EmptyNetworkService> receiver_{this};
};

std::unique_ptr<network::mojom::EmptyNetworkService> RunEmptyNetworkService(
    mojo::PendingReceiver<network::mojom::EmptyNetworkService> receiver) {
  return std::make_unique<EmptyNetworkService>(std::move(receiver));
}
}  // namespace

void RegisterEmptyNetworkService(mojo::ServiceFactory& services) {
  services.Add(RunEmptyNetworkService);
}
}  // namespace network
