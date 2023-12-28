// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_fido_device_factory.h"

#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_fido_device_discovery.h"

namespace device::test {

VirtualFidoDeviceFactory::VirtualFidoDeviceFactory() = default;
VirtualFidoDeviceFactory::~VirtualFidoDeviceFactory() = default;

void VirtualFidoDeviceFactory::SetSupportedProtocol(
    ProtocolVersion supported_protocol) {
  supported_protocol_ = supported_protocol;
}

void VirtualFidoDeviceFactory::SetTransport(FidoTransportProtocol transport) {
  transport_ = transport;
  state_->transport = transport;
}

void VirtualFidoDeviceFactory::SetCtap2Config(
    const VirtualCtap2Device::Config& config) {
  supported_protocol_ = ProtocolVersion::kCtap2;
  ctap2_config_ = config;
}

VirtualFidoDevice::State* VirtualFidoDeviceFactory::mutable_state() {
  return state_.get();
}

scoped_refptr<VirtualFidoDeviceDiscovery::Trace>
VirtualFidoDeviceFactory::trace() {
  return trace_;
}

std::vector<std::unique_ptr<FidoDiscoveryBase>>
VirtualFidoDeviceFactory::Create(FidoTransportProtocol transport) {
  if (transport != transport_) {
    return {};
  }
  const size_t trace_index = trace_->discoveries.size();
  trace_->discoveries.emplace_back();
  return SingleDiscovery(std::make_unique<VirtualFidoDeviceDiscovery>(
      trace_, trace_index, transport_, state_, supported_protocol_,
      ctap2_config_, /*disconnect_events=*/nullptr,
      std::move(contact_device_stream_)));
}

bool VirtualFidoDeviceFactory::IsTestOverride() {
  return true;
}

base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)>
VirtualFidoDeviceFactory::get_cable_contact_callback() {
  base::RepeatingCallback<void(std::unique_ptr<cablev2::Pairing>)> ret;
  std::tie(ret, contact_device_stream_) =
      FidoDiscoveryBase::EventStream<std::unique_ptr<cablev2::Pairing>>::New();
  return ret;
}

void VirtualFidoDeviceFactory::set_discover_win_webauthn_api_authenticator(
    bool on) {
  discover_win_webauthn_api_authenticator_ = on;
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<device::FidoDiscoveryBase>
VirtualFidoDeviceFactory::MaybeCreateWinWebAuthnApiDiscovery() {
  if (!discover_win_webauthn_api_authenticator_) {
    return nullptr;
  }

  return FidoDiscoveryFactory::MaybeCreateWinWebAuthnApiDiscovery();
}
#endif

}  // namespace device::test
