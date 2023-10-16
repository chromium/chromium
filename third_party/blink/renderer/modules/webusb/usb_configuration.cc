// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb_configuration.h"

#include "services/device/public/mojom/usb_device.mojom-blink.h"
#include "third_party/blink/renderer/modules/webusb/usb_device.h"
#include "third_party/blink/renderer/modules/webusb/usb_interface.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

USBConfiguration* USBConfiguration::Create(const USBDevice* device,
                                           wtf_size_t configuration_index) {
  return MakeGarbageCollected<USBConfiguration>(device, configuration_index);
}

USBConfiguration* USBConfiguration::Create(const USBDevice* device,
                                           uint8_t configuration_value,
                                           ExceptionState& exception_state) {
  const auto& configurations = device->Info().configurations;
  for (wtf_size_t i = 0; i < configurations.size(); ++i) {
    if (configurations[i]->configuration_value == configuration_value)
      return MakeGarbageCollected<USBConfiguration>(device, i);
  }
  exception_state.ThrowRangeError("Invalid configuration value.");
  return nullptr;
}

USBConfiguration::USBConfiguration(const USBDevice* device,
                                   wtf_size_t configuration_index)
    : device_(device), configuration_index_(configuration_index) {
  DCHECK(device_);
  DCHECK_LT(configuration_index_, device_->Info().configurations.size());

  for (wtf_size_t i = 0; i < Info().interfaces.size(); ++i)
    interfaces_.push_back(USBInterface::Create(this, i));
}

const USBDevice* USBConfiguration::Device() const {
  return device_.Get();
}

wtf_size_t USBConfiguration::Index() const {
  return configuration_index_;
}

const device::mojom::blink::UsbConfigurationInfo& USBConfiguration::Info()
    const {
  return *device_->Info().configurations[configuration_index_];
}

HeapVector<Member<USBInterface>> USBConfiguration::interfaces() const {
  return interfaces_;
}

void USBConfiguration::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(interfaces_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
