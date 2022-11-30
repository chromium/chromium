// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/test/test_service.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/mojom/connector.mojom.h"

namespace service_manager {

TestService::TestService(mojo::PendingReceiver<mojom::Service> receiver)
    : receiver_(this, std::move(receiver)) {
  // Run until we have a functioning Connector end-to-end, in case e.g. the test
  // wants to making blocking calls on interfaces right away.
  mojo::Remote<mojom::Connector> flushing_connector;
  receiver_.GetConnector()->BindConnectorReceiver(
      flushing_connector.BindNewPipeAndPassReceiver());
  flushing_connector.FlushForTesting();
}

TestService::~TestService() = default;

}  // namespace service_manager
