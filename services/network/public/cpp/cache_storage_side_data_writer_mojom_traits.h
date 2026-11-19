// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CACHE_STORAGE_SIDE_DATA_WRITER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CACHE_STORAGE_SIDE_DATA_WRITER_MOJOM_TRAITS_H_

#include <cstdint>
#include <optional>

#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace network {

enum class CacheStorageSideDataWriterTag {
  kDefault,
};

namespace mojom {
enum class CacheStorageSideDataWriterTag : int32_t;
class CacheStorageSideDataWriter;
}  // namespace mojom

}  // namespace network

namespace mojo {

template <>
struct EnumTraits<network::mojom::CacheStorageSideDataWriterTag,
                  network::CacheStorageSideDataWriterTag> {
  static network::mojom::CacheStorageSideDataWriterTag ToMojom(
      network::CacheStorageSideDataWriterTag input) {
    return static_cast<network::mojom::CacheStorageSideDataWriterTag>(0);
  }

  static std::optional<network::CacheStorageSideDataWriterTag> FromMojom(
      network::mojom::CacheStorageSideDataWriterTag input) {
    return network::CacheStorageSideDataWriterTag::kDefault;
  }
};

template <>
struct CloneTraits<PendingRemote<network::mojom::CacheStorageSideDataWriter>> {
  static PendingRemote<network::mojom::CacheStorageSideDataWriter> Clone(
      const PendingRemote<network::mojom::CacheStorageSideDataWriter>& input) {
    return PendingRemote<network::mojom::CacheStorageSideDataWriter>();
  }
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CACHE_STORAGE_SIDE_DATA_WRITER_MOJOM_TRAITS_H_
