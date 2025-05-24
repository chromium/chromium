// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb_endpoint.h"

#include "services/device/public/mojom/usb_device.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_direction.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_endpoint_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webusb/usb_alternate_interface.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

using device::mojom::blink::UsbTransferType;
using device::mojom::blink::UsbTransferDirection;

namespace blink {

namespace {

V8USBDirection::Enum ConvertDirectionToEnum(
    const UsbTransferDirection& direction) {
  switch (direction) {
    case UsbTransferDirection::INBOUND:
      return V8USBDirection::Enum::kIn;
    case UsbTransferDirection::OUTBOUND:
      return V8USBDirection::Enum::kOut;
  }
  NOTREACHED();
}

V8USBEndpointType::Enum ConvertTypeToEnum(const UsbTransferType& type) {
  switch (type) {
    case UsbTransferType::BULK:
      return V8USBEndpointType::Enum::kBulk;
    case UsbTransferType::INTERRUPT:
      return V8USBEndpointType::Enum::kInterrupt;
    case UsbTransferType::ISOCHRONOUS:
      return V8USBEndpointType::Enum::kIsochronous;
    case UsbTransferType::CONTROL:
      // Should not happen.
      break;
  }
  NOTREACHED();
}

}  // namespace

USBEndpoint* USBEndpoint::Create(const USBAlternateInterface* alternate,
                                 wtf_size_t endpoint_index) {
  return MakeGarbageCollected<USBEndpoint>(alternate, endpoint_index);
}

USBEndpoint* USBEndpoint::Create(const USBAlternateInterface* alternate,
                                 uint8_t endpoint_number,
                                 const V8USBDirection& direction,
                                 ExceptionState& exception_state) {
  UsbTransferDirection mojo_direction = direction == V8USBDirection::Enum::kIn
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

V8USBDirection USBEndpoint::direction() const {
  return V8USBDirection(ConvertDirectionToEnum(Info().direction));
}

V8USBEndpointType USBEndpoint::type() const {
  return V8USBEndpointType(ConvertTypeToEnum(Info().type));
}

void USBEndpoint::Trace(Visitor* visitor) const {
  visitor->Trace(alternate_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
