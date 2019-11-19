// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MOCK_FIDO_DISCOVERY_OBSERVER_H_
#define DEVICE_FIDO_MOCK_FIDO_DISCOVERY_OBSERVER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "device/fido/fido_device_discovery.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class FidoAuthenticator;

class MockFidoDiscoveryObserver : public FidoDiscoveryBase::Observer {
 public:
  MockFidoDiscoveryObserver();
  ~MockFidoDiscoveryObserver() override;

  MOCK_METHOD3(DiscoveryStarted,
               void(FidoDiscoveryBase*, bool, std::vector<FidoAuthenticator*>));
  MOCK_METHOD2(DiscoveryStopped, void(FidoDiscoveryBase*, bool));
  MOCK_METHOD2(AuthenticatorAdded,
               void(FidoDiscoveryBase*, FidoAuthenticator*));
  MOCK_METHOD2(AuthenticatorRemoved,
               void(FidoDiscoveryBase*, FidoAuthenticator*));
  MOCK_METHOD3(AuthenticatorIdChanged,
               void(FidoDiscoveryBase*, const std::string&, std::string));
  MOCK_METHOD3(AuthenticatorPairingModeChanged,
               void(FidoDiscoveryBase*, const std::string&, bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFidoDiscoveryObserver);
};

}  // namespace device

#endif  // DEVICE_FIDO_MOCK_FIDO_DISCOVERY_OBSERVER_H_
