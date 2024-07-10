// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_DIGITAL_IDENTITY_REQUEST_HANDLER_H_
#define DEVICE_FIDO_DIGITAL_IDENTITY_REQUEST_HANDLER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_request_handler_base.h"

namespace device {

class FidoAuthenticator;
class FidoDiscoveryFactory;

// Handles discovery for digital credentials API.
class COMPONENT_EXPORT(DEVICE_FIDO) DigitalIdentityRequestHandler
    : public FidoRequestHandlerBase {
 public:
  explicit DigitalIdentityRequestHandler(
      FidoDiscoveryFactory* fido_discovery_factory);

  DigitalIdentityRequestHandler(const DigitalIdentityRequestHandler&) = delete;
  DigitalIdentityRequestHandler& operator=(
      const DigitalIdentityRequestHandler&) = delete;

  ~DigitalIdentityRequestHandler() override;

 private:
  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;
};

}  // namespace device

#endif  // DEVICE_FIDO_DIGITAL_IDENTITY_REQUEST_HANDLER_H_
