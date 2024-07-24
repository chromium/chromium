// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/digital_identity_request_handler.h"

#include "base/functional/bind.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_types.h"

namespace device {

DigitalIdentityRequestHandler::DigitalIdentityRequestHandler(
    FidoDiscoveryFactory* fido_discovery_factory) {
  transport_availability_info().request_type = FidoRequestType::kMakeCredential;
  base::flat_set<FidoTransportProtocol> allowed_transports;
  allowed_transports.insert(FidoTransportProtocol::kHybrid);

  InitDiscoveries(fido_discovery_factory, /*additional_discoveries=*/{},
                  allowed_transports,
                  /*consider_enclave=*/false);
  Start();
}

DigitalIdentityRequestHandler::~DigitalIdentityRequestHandler() = default;

void DigitalIdentityRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  // TODO(crbug.com/332562244): Implement
}

}  // namespace device
