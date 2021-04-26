// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DISCOVERY_BASE_H_
#define DEVICE_FIDO_FIDO_DISCOVERY_BASE_H_

#include <vector>

#include <ostream>

#include "base/check.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

class FidoAuthenticator;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoDiscoveryBase {
 public:
  virtual ~FidoDiscoveryBase();

  class COMPONENT_EXPORT(DEVICE_FIDO) Observer {
   public:
    virtual ~Observer();

    // It is guaranteed that this is never invoked synchronously from Start().
    // |authenticators| is the list of authenticators discovered upon start.
    virtual void DiscoveryStarted(
        FidoDiscoveryBase* discovery,
        bool success,
        std::vector<FidoAuthenticator*> authenticators = {}) {}

    // Called after DiscoveryStarted for any devices discovered after
    // initialization.
    virtual void AuthenticatorAdded(FidoDiscoveryBase* discovery,
                                    FidoAuthenticator* authenticator) = 0;
    virtual void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                                      FidoAuthenticator* authenticator) = 0;
  };

  // Start authenticator discovery. The Observer must have been set before this
  // method is invoked. DiscoveryStarted must be invoked asynchronously from
  // this method.
  virtual void Start() = 0;

  // Attempts to stop discovery and returns whether stopping was successful.
  virtual bool MaybeStop();

  Observer* observer() const { return observer_; }
  void set_observer(Observer* observer) {
    DCHECK(!observer_ || !observer) << "Only one observer is supported.";
    observer_ = observer;
  }
  FidoTransportProtocol transport() const { return transport_; }

 protected:
  FidoDiscoveryBase(FidoTransportProtocol transport);

 private:
  const FidoTransportProtocol transport_;
  Observer* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FidoDiscoveryBase);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DISCOVERY_BASE_H_
