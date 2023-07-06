// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HASH_VALUE_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HASH_VALUE_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "net/base/hash_value.h"
#include "services/network/public/mojom/hash_value.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::SHA256HashValueDataView,
                    net::SHA256HashValue> {
  static base::span<const uint8_t, 32> data(const net::SHA256HashValue& value) {
    return value.data;
  }

  static bool Read(network::mojom::SHA256HashValueDataView in,
                   net::SHA256HashValue* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HASH_VALUE_MOJOM_TRAITS_H_
