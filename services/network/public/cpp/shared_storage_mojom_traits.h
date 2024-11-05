// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SHARED_STORAGE_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SHARED_STORAGE_MOJOM_TRAITS_H_

#include <string>

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "services/network/public/mojom/shared_storage.mojom.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_SHARED_STORAGE)
    StructTraits<network::mojom::SharedStorageKeyArgumentDataView,
                 std::u16string> {
  static bool Read(network::mojom::SharedStorageKeyArgumentDataView data,
                   std::u16string* out_key);

  static const std::u16string& data(const std::u16string& input) {
    return input;
  }
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_SHARED_STORAGE)
    StructTraits<network::mojom::SharedStorageValueArgumentDataView,
                 std::u16string> {
  static bool Read(network::mojom::SharedStorageValueArgumentDataView data,
                   std::u16string* out_value);

  static const std::u16string& data(const std::u16string& input) {
    return input;
  }
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SHARED_STORAGE_MOJOM_TRAITS_H_
