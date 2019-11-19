// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_DEVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_DEVICE_H_

#include <bitset>
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ScriptPromiseResolver;
class ScriptState;
class USBConfiguration;
class USBControlTransferParameters;

class USBDevice : public ScriptWrappable, public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(USBDevice);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBDevice* Create(
      device::mojom::blink::UsbDeviceInfoPtr device_info,
      mojo::PendingRemote<device::mojom::blink::UsbDevice> device,
      ExecutionContext* context) {
    return MakeGarbageCollected<USBDevice>(std::move(device_info),
                                           std::move(device), context);
  }

  explicit USBDevice(device::mojom::blink::UsbDeviceInfoPtr,
                     mojo::PendingRemote<device::mojom::blink::UsbDevice>,
                     ExecutionContext*);
  ~USBDevice() override;

  const device::mojom::blink::UsbDeviceInfo& Info() const {
    return *device_info_;
  }
  bool IsInterfaceClaimed(wtf_size_t configuration_index,
                          wtf_size_t interface_index) const;
  wtf_size_t SelectedAlternateInterface(wtf_size_t interface_index) const;

  // USBDevice.idl
  uint8_t usbVersionMajor() const { return Info().usb_version_major; }
  uint8_t usbVersionMinor() const { return Info().usb_version_minor; }
  uint8_t usbVersionSubminor() const { return Info().usb_version_subminor; }
  uint8_t deviceClass() const { return Info().class_code; }
  uint8_t deviceSubclass() const { return Info().subclass_code; }
  uint8_t deviceProtocol() const { return Info().protocol_code; }
  uint16_t vendorId() const { return Info().vendor_id; }
  uint16_t productId() const { return Info().product_id; }
  uint8_t deviceVersionMajor() const { return Info().device_version_major; }
  uint8_t deviceVersionMinor() const { return Info().device_version_minor; }
  uint8_t deviceVersionSubminor() const {
    return Info().device_version_subminor;
  }
  String manufacturerName() const { return Info().manufacturer_name; }
  String productName() const { return Info().product_name; }
  String serialNumber() const { return Info().serial_number; }
  USBConfiguration* configuration() const;
  HeapVector<Member<USBConfiguration>> configurations() const;
  bool opened() const { return opened_; }

  ScriptPromise open(ScriptState*);
  ScriptPromise close(ScriptState*);
  ScriptPromise selectConfiguration(ScriptState*, uint8_t configuration_value);
  ScriptPromise claimInterface(ScriptState*, uint8_t interface_number);
  ScriptPromise releaseInterface(ScriptState*, uint8_t interface_number);
  ScriptPromise selectAlternateInterface(ScriptState*,
                                         uint8_t interface_number,
                                         uint8_t alternate_setting);
  ScriptPromise controlTransferIn(ScriptState*,
                                  const USBControlTransferParameters* setup,
                                  unsigned length);
  ScriptPromise controlTransferOut(ScriptState*,
                                   const USBControlTransferParameters* setup);
  ScriptPromise controlTransferOut(ScriptState*,
                                   const USBControlTransferParameters* setup,
                                   const ArrayBufferOrArrayBufferView& data);
  ScriptPromise clearHalt(ScriptState*,
                          String direction,
                          uint8_t endpoint_number);
  ScriptPromise transferIn(ScriptState*,
                           uint8_t endpoint_number,
                           unsigned length);
  ScriptPromise transferOut(ScriptState*,
                            uint8_t endpoint_number,
                            const ArrayBufferOrArrayBufferView& data);
  ScriptPromise isochronousTransferIn(ScriptState*,
                                      uint8_t endpoint_number,
                                      Vector<unsigned> packet_lengths);
  ScriptPromise isochronousTransferOut(ScriptState*,
                                       uint8_t endpoint_number,
                                       const ArrayBufferOrArrayBufferView& data,
                                       Vector<unsigned> packet_lengths);
  ScriptPromise reset(ScriptState*);

  // ContextLifecycleObserver interface.
  void ContextDestroyed(ExecutionContext*) override;

  void Trace(blink::Visitor*) override;

 private:
  static const size_t kEndpointsBitsNumber = 16;

  wtf_size_t FindConfigurationIndex(uint8_t configuration_value) const;
  wtf_size_t FindInterfaceIndex(uint8_t interface_number) const;
  wtf_size_t FindAlternateIndex(wtf_size_t interface_index,
                                uint8_t alternate_setting) const;
  bool IsProtectedInterfaceClass(wtf_size_t interface_index) const;
  bool IsClassWhitelistedForExtension(uint8_t class_code) const;
  bool EnsureNoDeviceChangeInProgress(ScriptPromiseResolver*) const;
  bool EnsureNoDeviceOrInterfaceChangeInProgress(ScriptPromiseResolver*) const;
  bool EnsureDeviceConfigured(ScriptPromiseResolver*) const;
  bool EnsureInterfaceClaimed(uint8_t interface_number,
                              ScriptPromiseResolver*) const;
  bool EnsureEndpointAvailable(bool in_transfer,
                               uint8_t endpoint_number,
                               ScriptPromiseResolver*) const;
  bool AnyInterfaceChangeInProgress() const;
  device::mojom::blink::UsbControlTransferParamsPtr
  ConvertControlTransferParameters(const USBControlTransferParameters*,
                                   ScriptPromiseResolver*) const;
  void SetEndpointsForInterface(wtf_size_t interface_index, bool set);

  void AsyncOpen(ScriptPromiseResolver*,
                 device::mojom::blink::UsbOpenDeviceError);
  void AsyncClose(ScriptPromiseResolver*);
  void OnDeviceOpenedOrClosed(bool);
  void AsyncSelectConfiguration(wtf_size_t configuration_index,
                                ScriptPromiseResolver*,
                                bool success);
  void OnConfigurationSelected(bool success, wtf_size_t configuration_index);
  void AsyncClaimInterface(wtf_size_t interface_index,
                           ScriptPromiseResolver*,
                           bool success);
  void AsyncReleaseInterface(wtf_size_t interface_index,
                             ScriptPromiseResolver*,
                             bool success);
  void OnInterfaceClaimedOrUnclaimed(bool claimed, wtf_size_t interface_index);
  void AsyncSelectAlternateInterface(wtf_size_t interface_index,
                                     wtf_size_t alternate_index,
                                     ScriptPromiseResolver*,
                                     bool success);
  void AsyncControlTransferIn(ScriptPromiseResolver*,
                              device::mojom::blink::UsbTransferStatus,
                              const Vector<uint8_t>&);
  void AsyncControlTransferOut(unsigned,
                               ScriptPromiseResolver*,
                               device::mojom::blink::UsbTransferStatus);
  void AsyncClearHalt(ScriptPromiseResolver*, bool success);
  void AsyncTransferIn(ScriptPromiseResolver*,
                       device::mojom::blink::UsbTransferStatus,
                       const Vector<uint8_t>&);
  void AsyncTransferOut(unsigned,
                        ScriptPromiseResolver*,
                        device::mojom::blink::UsbTransferStatus);
  void AsyncIsochronousTransferIn(
      ScriptPromiseResolver*,
      const Vector<uint8_t>&,
      Vector<device::mojom::blink::UsbIsochronousPacketPtr>);
  void AsyncIsochronousTransferOut(
      ScriptPromiseResolver*,
      Vector<device::mojom::blink::UsbIsochronousPacketPtr>);
  void AsyncReset(ScriptPromiseResolver*, bool success);

  void OnConnectionError();
  bool MarkRequestComplete(ScriptPromiseResolver*);

  device::mojom::blink::UsbDeviceInfoPtr device_info_;
  mojo::Remote<device::mojom::blink::UsbDevice> device_;
  HeapHashSet<Member<ScriptPromiseResolver>> device_requests_;
  bool opened_;
  bool device_state_change_in_progress_;
  wtf_size_t configuration_index_;

  // These vectors have one entry for each interface in the currently selected
  // configured. Use the index returned by FindInterfaceIndex().
  WTF::Vector<bool> claimed_interfaces_;
  WTF::Vector<bool> interface_state_change_in_progress_;
  WTF::Vector<wtf_size_t> selected_alternates_;

  // These bit sets have one entry for each endpoint. Index using the endpoint
  // number (lower 4 bits of the endpoint address).
  std::bitset<kEndpointsBitsNumber> in_endpoints_;
  std::bitset<kEndpointsBitsNumber> out_endpoints_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_DEVICE_H_
