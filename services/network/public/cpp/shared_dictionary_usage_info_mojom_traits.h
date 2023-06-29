// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SHARED_DICTIONARY_USAGE_INFO_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SHARED_DICTIONARY_USAGE_INFO_MOJOM_TRAITS_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/extras/shared_dictionary/shared_dictionary_usage_info.h"
#include "services/network/public/mojom/shared_dictionary_usage_info.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::SharedDictionaryUsageInfoDataView,
                    net::SharedDictionaryUsageInfo> {
  static const net::SharedDictionaryIsolationKey& isolation_key(
      const net::SharedDictionaryUsageInfo& obj) {
    return obj.isolation_key;
  }

  static uint64_t total_size_bytes(const net::SharedDictionaryUsageInfo& obj) {
    return obj.total_size_bytes;
  }

  static bool Read(network::mojom::SharedDictionaryUsageInfoDataView data,
                   net::SharedDictionaryUsageInfo* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SHARED_DICTIONARY_USAGE_INFO_MOJOM_TRAITS_H_
