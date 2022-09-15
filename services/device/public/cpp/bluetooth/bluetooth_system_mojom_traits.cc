// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/bluetooth/bluetooth_system_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    device::mojom::BluetoothAddressDataView,
    std::array<uint8_t, 6>>::Read(device::mojom::BluetoothAddressDataView data,
                                  std::array<uint8_t, 6>* out_address) {
  ArrayDataView<uint8_t> address;
  data.GetAddressDataView(&address);
  if (address.is_null())
    return false;

  // The size is validated by the generated validation code.
  DCHECK_EQ(6u, address.size());

  std::copy_n(address.data(), 6, std::begin(*out_address));

  return true;
}

}  // namespace mojo
