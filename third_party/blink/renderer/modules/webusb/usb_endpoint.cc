// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb_endpoint.h"

#include "services/device/public/mojom/usb_device.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webusb/usb_alternate_interface.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

using device::mojom::blink::UsbTransferType;
using device::mojom::blink::UsbTransferDirection;

namespace blink {

namespace {

String ConvertDirectionToEnum(const UsbTransferDirection& direction) {
  switch (direction) {
    case UsbTransferDirection::INBOUND:
      return "in";
    case UsbTransferDirection::OUTBOUND:
      return "out";
    default:
      NOTREACHED();
      return "";
  }
}

String ConvertTypeToEnum(const UsbTransferType& type) {
  switch (type) {
    case UsbTransferType::BULK:
      return "bulk";
    case UsbTransferType::INTERRUPT:
      return "interrupt";
    case UsbTransferType::ISOCHRONOUS:
      return "isochronous";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

USBEndpoint* USBEndpoint::Create(const USBAlternateInterface* alternate,
                                 wtf_size_t endpoint_index) {
  return MakeGarbageCollected<USBEndpoint>(alternate, endpoint_index);
}

USBEndpoint* USBEndpoint::Create(const USBAlternateInterface* alternate,
                                 uint8_t endpoint_number,
                                 const String& direction,
                                 ExceptionState& exception_state) {
  UsbTransferDirection mojo_direction = direction == "in"
                                            ? UsbTransferDirection::INBOUND
                                            : UsbTransferDirection::OUTBOUND;
  const auto& endpoints = alternate->Info().endpoints;
  for (wtf_size_t i = 0; i < endpoints.size(); ++i) {
    const auto& endpoint = endpoints[i];
    if (endpoint->endpoint_number == endpoint_number &&
        endpoint->direction == mojo_direction)
      return USBEndpoint::Create(alternate, i);
  }
  exception_state.ThrowRangeError(
      "No such endpoint exists in the given alternate interface.");
  return nullptr;
}

USBEndpoint::USBEndpoint(const USBAlternateInterface* alternate,
                         wtf_size_t endpoint_index)
    : alternate_(alternate), endpoint_index_(endpoint_index) {
  DCHECK(alternate_);
  DCHECK_LT(endpoint_index_, alternate_->Info().endpoints.size());
}

const device::mojom::blink::UsbEndpointInfo& USBEndpoint::Info() const {
  const device::mojom::blink::UsbAlternateInterfaceInfo& alternate_info =
      alternate_->Info();
  DCHECK_LT(endpoint_index_, alternate_info.endpoints.size());
  return *alternate_info.endpoints[endpoint_index_];
}

String USBEndpoint::direction() const {
  return ConvertDirectionToEnum(Info().direction);
}

String USBEndpoint::type() const {
  return ConvertTypeToEnum(Info().type);
}

void USBEndpoint::Trace(blink::Visitor* visitor) {
  visitor->Trace(alternate_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
