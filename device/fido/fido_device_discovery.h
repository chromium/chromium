// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DEVICE_DISCOVERY_H_
#define DEVICE_FIDO_FIDO_DEVICE_DISCOVERY_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

class FidoDevice;
class FidoDeviceAuthenticator;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoDeviceDiscovery
    : public FidoDiscoveryBase {
 public:
  enum class State {
    kIdle,
    kStarting,
    kRunning,
  };

  ~FidoDeviceDiscovery() override;

  bool is_start_requested() const { return state_ != State::kIdle; }
  bool is_running() const { return state_ == State::kRunning; }

  std::vector<FidoDeviceAuthenticator*> GetAuthenticatorsForTesting();
  std::vector<const FidoDeviceAuthenticator*> GetAuthenticatorsForTesting()
      const;
  FidoDeviceAuthenticator* GetAuthenticatorForTesting(
      base::StringPiece authenticator_id);

  // FidoDiscoveryBase:
  void Start() override;

 protected:
  FidoDeviceDiscovery(FidoTransportProtocol transport);

  void NotifyDiscoveryStarted(bool success);
  void NotifyAuthenticatorAdded(FidoAuthenticator* authenticator);
  void NotifyAuthenticatorRemoved(FidoAuthenticator* authenticator);

  bool AddDevice(std::unique_ptr<FidoDevice> device);
  bool RemoveDevice(base::StringPiece device_id);

  FidoDeviceAuthenticator* GetAuthenticator(base::StringPiece authenticator_id);

  // Subclasses should implement this to actually start the discovery when it is
  // requested.
  //
  // The implementation should asynchronously invoke NotifyDiscoveryStarted when
  // the discovery is s tarted.
  virtual void StartInternal() = 0;

  // Map of ID to authenticator. It is a guarantee to subclasses that the ID of
  // the authenticator equals the ID of the device.
  std::map<std::string, std::unique_ptr<FidoDeviceAuthenticator>, std::less<>>
      authenticators_;

 private:
  State state_ = State::kIdle;
  base::WeakPtrFactory<FidoDeviceDiscovery> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoDeviceDiscovery);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DEVICE_DISCOVERY_H_
