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
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace service_manager {
class Connector;
}

namespace device {

class FidoDevice;
class FidoDeviceAuthenticator;

namespace internal {
class ScopedFidoDiscoveryFactory;
}

class COMPONENT_EXPORT(DEVICE_FIDO) FidoDeviceDiscovery
    : public FidoDiscoveryBase {
 public:
  enum class State {
    kIdle,
    kStarting,
    kRunning,
  };

  // Factory functions to construct an instance that discovers authenticators on
  // the given |transport| protocol. The first variant is for everything except
  // for cloud-assisted BLE which is handled by the second variant.
  //
  // FidoTransportProtocol::kUsbHumanInterfaceDevice requires specifying a valid
  // |connector| on Desktop, and is not valid on Android.
  static std::unique_ptr<FidoDeviceDiscovery> Create(
      FidoTransportProtocol transport,
      ::service_manager::Connector* connector);
  static std::unique_ptr<FidoDeviceDiscovery> CreateCable(
      std::vector<CableDiscoveryData> cable_data);

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
  friend class internal::ScopedFidoDiscoveryFactory;

  // Factory function can be overridden by tests to construct fakes.
  using FactoryFuncPtr = decltype(&Create);
  using CableFactoryFuncPtr = decltype(&CreateCable);
  static FactoryFuncPtr g_factory_func_;
  static CableFactoryFuncPtr g_cable_factory_func_;

  State state_ = State::kIdle;
  base::WeakPtrFactory<FidoDeviceDiscovery> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FidoDeviceDiscovery);
};

namespace internal {

// Base class for a scoped override of FidoDeviceDiscovery::Create, used in unit
// tests, layout tests, and when running with the Web Authn Testing API enabled.
//
// While there is a subclass instance in scope, calls to the factory method will
// be hijacked such that the derived class's CreateFidoDiscovery method will be
// invoked instead.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedFidoDiscoveryFactory {
 public:
  // There should be at most one instance of any subclass in scope at a time.
  ScopedFidoDiscoveryFactory();
  virtual ~ScopedFidoDiscoveryFactory();

  const std::vector<CableDiscoveryData>& last_cable_data() const {
    return last_cable_data_;
  }

 protected:
  void set_last_cable_data(std::vector<CableDiscoveryData> cable_data) {
    last_cable_data_ = std::move(cable_data);
  }

  virtual std::unique_ptr<FidoDeviceDiscovery> CreateFidoDiscovery(
      FidoTransportProtocol transport,
      ::service_manager::Connector* connector) = 0;

 private:
  static std::unique_ptr<FidoDeviceDiscovery>
  ForwardCreateFidoDiscoveryToCurrentFactory(
      FidoTransportProtocol transport,
      ::service_manager::Connector* connector);

  static std::unique_ptr<FidoDeviceDiscovery>
  ForwardCreateCableDiscoveryToCurrentFactory(
      std::vector<CableDiscoveryData> cable_data);

  static ScopedFidoDiscoveryFactory* g_current_factory;

  FidoDeviceDiscovery::FactoryFuncPtr original_factory_func_;
  FidoDeviceDiscovery::CableFactoryFuncPtr original_cable_factory_func_;
  std::vector<CableDiscoveryData> last_cable_data_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFidoDiscoveryFactory);
};

}  // namespace internal
}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DEVICE_DISCOVERY_H_
