// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb_alternate_interface.h"

#include "third_party/blink/renderer/modules/webusb/usb_endpoint.h"
#include "third_party/blink/renderer/modules/webusb/usb_interface.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

USBAlternateInterface* USBAlternateInterface::Create(
    const USBInterface* interface,
    wtf_size_t alternate_index) {
  return MakeGarbageCollected<USBAlternateInterface>(interface,
                                                     alternate_index);
}

USBAlternateInterface* USBAlternateInterface::Create(
    const USBInterface* interface,
    uint8_t alternate_setting,
    ExceptionState& exception_state) {
  const auto& alternates = interface->Info().alternates;
  for (wtf_size_t i = 0; i < alternates.size(); ++i) {
    if (alternates[i]->alternate_setting == alternate_setting)
      return USBAlternateInterface::Create(interface, i);
  }
  exception_state.ThrowRangeError("Invalid alternate setting.");
  return nullptr;
}

USBAlternateInterface::USBAlternateInterface(const USBInterface* interface,
                                             wtf_size_t alternate_index)
    : interface_(interface), alternate_index_(alternate_index) {
  DCHECK(interface_);
  DCHECK_LT(alternate_index_, interface_->Info().alternates.size());
}

const device::mojom::blink::UsbAlternateInterfaceInfo&
USBAlternateInterface::Info() const {
  const device::mojom::blink::UsbInterfaceInfo& interface_info =
      interface_->Info();
  DCHECK_LT(alternate_index_, interface_info.alternates.size());
  return *interface_info.alternates[alternate_index_];
}

HeapVector<Member<USBEndpoint>> USBAlternateInterface::endpoints() const {
  HeapVector<Member<USBEndpoint>> endpoints;
  for (wtf_size_t i = 0; i < Info().endpoints.size(); ++i) {
    // Filter out control endpoints because there is no coresponding enum value
    // defined in WebUSB.
    if (Info().endpoints[i]->type !=
        device::mojom::blink::UsbTransferType::CONTROL) {
      endpoints.push_back(USBEndpoint::Create(this, i));
    }
  }
  return endpoints;
}

void USBAlternateInterface::Trace(blink::Visitor* visitor) {
  visitor->Trace(interface_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
