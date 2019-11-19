// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_ENDPOINT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_ENDPOINT_H_

#include "services/device/public/mojom/usb_device.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class ExceptionState;
class USBAlternateInterface;

class USBEndpoint : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBEndpoint* Create(const USBAlternateInterface*,
                             wtf_size_t endpoint_index);
  static USBEndpoint* Create(const USBAlternateInterface*,
                             uint8_t endpoint_number,
                             const String& direction,
                             ExceptionState&);

  USBEndpoint(const USBAlternateInterface*, wtf_size_t endpoint_index);

  const device::mojom::blink::UsbEndpointInfo& Info() const;

  uint8_t endpointNumber() const { return Info().endpoint_number; }
  String direction() const;
  String type() const;
  unsigned packetSize() const { return Info().packet_size; }

  void Trace(blink::Visitor*) override;

 private:
  Member<const USBAlternateInterface> alternate_;
  const wtf_size_t endpoint_index_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_ENDPOINT_H_
