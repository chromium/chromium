// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_FACTORY_H_
#define DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_FACTORY_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_discovery.h"

namespace device::test {

// A |FidoDiscoveryFactory| that always returns |VirtualFidoDevice|s.
//
// If the transport is set to `hybrid` and a client obtains a phone contact
// callback from |get_cable_contact_callback|, authenticators will only be
// created only after the callback is executed.
class VirtualFidoDeviceFactory : public device::FidoDiscoveryFactory {
 public:
  VirtualFidoDeviceFactory();

  VirtualFidoDeviceFactory(const VirtualFidoDeviceFactory&) = delete;
  VirtualFidoDeviceFactory& operator=(const VirtualFidoDeviceFactory&) = delete;

  ~VirtualFidoDeviceFactory() override;

  // Sets the FidoTransportProtocol of the FidoDiscovery to be instantiated by
  // this VirtualFidoDeviceFactory. The default is
  // FidoTransportProtocol::kUsbHumanInterfaceDevice.
  //
  // The FidoTransportProtocol of the device instantiated by the FidoDiscovery
  // must be set separately in mutable_state().
  void SetTransport(FidoTransportProtocol transport);

  void SetSupportedProtocol(ProtocolVersion supported_protocol);
  // SetCtap2Config sets the configuration for |VirtualCtap2Device|s and sets
  // the supported protocol to CTAP2.
  void SetCtap2Config(const VirtualCtap2Device::Config& config);
  VirtualFidoDevice::State* mutable_state();
  scoped_refptr<VirtualFidoDeviceDiscovery::Trace> trace();

  // set_discover_win_webauthn_api_authenticator controls whether the
  // WebWebAuthnApi authenticator will be discovered. Create a
  // `WinWebAuthnApi::ScopedOverride` before settings to true.
  void set_discover_win_webauthn_api_authenticator(bool on);

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<device::FidoDiscoveryBase>
  MaybeCreateWinWebAuthnApiDiscovery() override;
#endif

 protected:
  // device::FidoDiscoveryFactory:
  std::vector<std::unique_ptr<FidoDiscoveryBase>> Create(
      FidoTransportProtocol transport) override;
  bool IsTestOverride() override;
  base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)>
  get_cable_contact_callback() override;

 private:
  std::unique_ptr<
      FidoDiscoveryBase::EventStream<std::unique_ptr<cablev2::Pairing>>>
      contact_device_stream_;
  ProtocolVersion supported_protocol_ = ProtocolVersion::kU2f;
  FidoTransportProtocol transport_ =
      FidoTransportProtocol::kUsbHumanInterfaceDevice;
  VirtualCtap2Device::Config ctap2_config_;
  scoped_refptr<VirtualFidoDevice::State> state_ = new VirtualFidoDevice::State;
  scoped_refptr<VirtualFidoDeviceDiscovery::Trace> trace_ =
      new VirtualFidoDeviceDiscovery::Trace;
  bool discover_win_webauthn_api_authenticator_ = false;

  base::WeakPtrFactory<VirtualFidoDeviceFactory> weak_ptr_factory_{this};
};

}  // namespace device::test

#endif  // DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_FACTORY_H_
