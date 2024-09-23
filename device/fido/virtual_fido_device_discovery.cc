// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_fido_device_discovery.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_fido_device_authenticator.h"
#include "device/fido/virtual_u2f_device.h"

namespace device::test {

VirtualFidoDeviceDiscovery::Trace::Trace() = default;
VirtualFidoDeviceDiscovery::Trace::~Trace() = default;

VirtualFidoDeviceDiscovery::VirtualFidoDeviceDiscovery(
    scoped_refptr<Trace> trace,
    size_t trace_index,
    FidoTransportProtocol transport,
    scoped_refptr<VirtualFidoDevice::State> state,
    ProtocolVersion supported_protocol,
    const VirtualCtap2Device::Config& ctap2_config,
    std::unique_ptr<FidoDeviceDiscovery::EventStream<bool>> disconnect_events,
    std::unique_ptr<
        FidoDeviceDiscovery::EventStream<std::unique_ptr<cablev2::Pairing>>>
        contact_device_stream)
    : FidoDeviceDiscovery(transport),
      trace_(std::move(trace)),
      trace_index_(trace_index),
      state_(std::move(state)),
      supported_protocol_(supported_protocol),
      ctap2_config_(ctap2_config),
      disconnect_events_(std::move(disconnect_events)),
      contact_device_stream_(std::move(contact_device_stream)) {}

VirtualFidoDeviceDiscovery::~VirtualFidoDeviceDiscovery() {
  trace_->discoveries[trace_index_].is_destroyed = true;
}

void VirtualFidoDeviceDiscovery::StartInternal() {
  // If the caller has passed a contact device stream, add a hybrid device
  // only after a notification is received.
  if (transport() == FidoTransportProtocol::kHybrid && contact_device_stream_) {
    contact_device_stream_->Connect(
        base::BindRepeating(&VirtualFidoDeviceDiscovery::AddVirtualDeviceAsync,
                            base::Unretained(this)));
  } else {
    AddVirtualDevice();
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&VirtualFidoDeviceDiscovery::NotifyDiscoveryStarted,
                     weak_ptr_factory_.GetWeakPtr(), true /* success */));

  if (disconnect_events_) {
    // |disconnect_events_| is owned by this object therefore, when this object
    // is destroyed, no more events can be received. Therefore |Unretained|
    // works here.
    disconnect_events_->Connect(base::BindRepeating(
        &VirtualFidoDeviceDiscovery::Disconnect, base::Unretained(this)));
  }
}

void VirtualFidoDeviceDiscovery::AddVirtualDeviceAsync(
    std::unique_ptr<cablev2::Pairing> _) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&VirtualFidoDeviceDiscovery::AddVirtualDevice,
                                weak_ptr_factory_.GetWeakPtr()));
}

void VirtualFidoDeviceDiscovery::AddVirtualDevice() {
  std::unique_ptr<VirtualFidoDevice> device;
  if (supported_protocol_ == ProtocolVersion::kCtap2) {
    device = std::make_unique<VirtualCtap2Device>(state_, ctap2_config_);
  } else {
    device = std::make_unique<VirtualU2fDevice>(state_);
  }

  id_ = device->GetId();
  auto authenticator =
      std::make_unique<VirtualFidoDeviceAuthenticator>(std::move(device));
  AddAuthenticator(std::move(authenticator));
}

void VirtualFidoDeviceDiscovery::Disconnect(bool _) {
  CHECK(!id_.empty());
  RemoveDevice(id_);
}

void VirtualFidoDeviceDiscovery::Stop() {
  trace_->discoveries[trace_index_].is_stopped = true;
  FidoDeviceDiscovery::Stop();
}

}  // namespace device::test
