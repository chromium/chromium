// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_SYSTEM_MOJOM_TRAITS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_SYSTEM_MOJOM_TRAITS_H_

#include <vector>

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace mojo {

template <>
class StructTraits<device::mojom::BluetoothAddressDataView,
                   std::array<uint8_t, 6>> {
 public:
  static base::span<const uint8_t, 6> address(
      const std::array<uint8_t, 6>& addr) {
    return base::make_span(addr);
  }

  static bool Read(device::mojom::BluetoothAddressDataView data,
                   std::array<uint8_t, 6>* out_address);
};

}  // namespace mojo

#endif  // SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_SYSTEM_MOJOM_TRAITS_H_
