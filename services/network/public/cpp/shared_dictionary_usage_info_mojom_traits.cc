// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_dictionary_usage_info_mojom_traits.h"

#include "services/network/public/cpp/shared_dictionary_isolation_key_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::SharedDictionaryUsageInfoDataView,
                  net::SharedDictionaryUsageInfo>::
    Read(network::mojom::SharedDictionaryUsageInfoDataView data,
         net::SharedDictionaryUsageInfo* out) {
  net::SharedDictionaryIsolationKey isolation_key;
  if (!data.ReadIsolationKey(&isolation_key)) {
    return false;
  }

  *out = net::SharedDictionaryUsageInfo{
      .isolation_key = isolation_key,
      .total_size_bytes = data.total_size_bytes()};
  return true;
}

}  // namespace mojo
