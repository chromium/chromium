// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb_interface.h"

#include "base/notreached.h"
#include "services/device/public/mojom/usb_device.mojom-blink.h"
#include "third_party/blink/renderer/modules/webusb/usb_alternate_interface.h"
#include "third_party/blink/renderer/modules/webusb/usb_configuration.h"
#include "third_party/blink/renderer/modules/webusb/usb_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

USBInterface* USBInterface::Create(const USBConfiguration* configuration,
                                   wtf_size_t interface_index) {
  return MakeGarbageCollected<USBInterface>(
      configuration->Device(), configuration->Index(), interface_index);
}

USBInterface* USBInterface::Create(const USBConfiguration* configuration,
                                   uint8_t interface_number,
                                   ExceptionState& exception_state) {
  const auto& interfaces = configuration->Info().interfaces;
  for (wtf_size_t i = 0; i < interfaces.size(); ++i) {
    if (interfaces[i]->interface_number == interface_number) {
      return MakeGarbageCollected<USBInterface>(configuration->Device(),
                                                configuration->Index(), i);
    }
  }
  exception_state.ThrowRangeError("Invalid interface index.");
  return nullptr;
}

USBInterface::USBInterface(const USBDevice* device,
                           wtf_size_t configuration_index,
                           wtf_size_t interface_index)
    : device_(device),
      configuration_index_(configuration_index),
      interface_index_(interface_index) {
  DCHECK_LT(configuration_index_, device_->Info().configurations.size());
  DCHECK_LT(
      interface_index_,
      device_->Info().configurations[configuration_index_]->interfaces.size());

  for (wtf_size_t i = 0; i < Info().alternates.size(); ++i)
    alternates_.push_back(USBAlternateInterface::Create(this, i));
}

const device::mojom::blink::UsbInterfaceInfo& USBInterface::Info() const {
  return *device_->Info()
              .configurations[configuration_index_]
              ->interfaces[interface_index_];
}

USBAlternateInterface* USBInterface::alternate() const {
  wtf_size_t index = 0;
  if (device_->IsInterfaceClaimed(configuration_index_, interface_index_)) {
    index = device_->SelectedAlternateInterfaceIndex(interface_index_);
  }
  // Every interface is guaranteed to have at least one alternate according
  // according to Interface Descriptor in section 9.6.5 of USB31 specification,
  // and how UsbInterfaceInfo is constructed by BuildUsbInterfaceInfoPtr() and
  // AggregateInterfacesForConfig() in services/device/usb/usb_descriptors.cc.
  DCHECK_LT(index, alternates_.size());
  return alternates_[index].Get();
}

HeapVector<Member<USBAlternateInterface>> USBInterface::alternates() const {
  return alternates_;
}

bool USBInterface::claimed() const {
  return device_->IsInterfaceClaimed(configuration_index_, interface_index_);
}

void USBInterface::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(alternates_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
