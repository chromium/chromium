// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_DISCOVERY_H_
#define DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_DISCOVERY_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/virtual_ctap2_device.h"

namespace device::test {

// A FidoDeviceDiscovery that always vends a single |VirtualFidoDevice|.
class VirtualFidoDeviceDiscovery final : public FidoDeviceDiscovery {
 public:
  // Trace contains a history of the discovery objects that have been created by
  // a given factory. VirtualFidoDeviceDiscovery gets a reference to this object
  // and keeps its information up to date.
  struct Trace : public base::RefCounted<Trace> {
    Trace();
    Trace(const Trace&) = delete;
    Trace& operator=(const Trace&) = delete;

    struct Discovery {
      bool is_stopped = false;
      bool is_destroyed = false;
    };
    std::vector<Discovery> discoveries;

   private:
    friend class base::RefCounted<Trace>;
    ~Trace();
  };

  VirtualFidoDeviceDiscovery(
      scoped_refptr<Trace> trace,
      size_t trace_index,
      FidoTransportProtocol transport,
      scoped_refptr<VirtualFidoDevice::State> state,
      ProtocolVersion supported_protocol,
      const VirtualCtap2Device::Config& ctap2_config,
      std::unique_ptr<EventStream<bool>> disconnect_events,
      std::unique_ptr<EventStream<std::unique_ptr<cablev2::Pairing>>>
          contact_device_stream = nullptr);
  ~VirtualFidoDeviceDiscovery() override;
  VirtualFidoDeviceDiscovery(const VirtualFidoDeviceDiscovery& other) = delete;
  VirtualFidoDeviceDiscovery& operator=(
      const VirtualFidoDeviceDiscovery& other) = delete;

 protected:
  void StartInternal() override;
  void Stop() override;

 private:
  void AddVirtualDeviceAsync(std::unique_ptr<cablev2::Pairing> _);
  void AddVirtualDevice();
  void Disconnect(bool _);

  scoped_refptr<Trace> trace_;
  const size_t trace_index_;
  scoped_refptr<VirtualFidoDevice::State> state_;
  const ProtocolVersion supported_protocol_;
  const VirtualCtap2Device::Config ctap2_config_;
  std::unique_ptr<EventStream<bool>> disconnect_events_;
  std::unique_ptr<EventStream<std::unique_ptr<cablev2::Pairing>>>
      contact_device_stream_;
  std::string id_;
  base::WeakPtrFactory<VirtualFidoDeviceDiscovery> weak_ptr_factory_{this};
};

}  // namespace device::test

#endif  // DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_DISCOVERY_H_
