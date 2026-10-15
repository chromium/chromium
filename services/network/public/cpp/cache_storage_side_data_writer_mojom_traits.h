// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CACHE_STORAGE_SIDE_DATA_WRITER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CACHE_STORAGE_SIDE_DATA_WRITER_MOJOM_TRAITS_H_

#include <concepts>
#include <cstdint>
#include <optional>

#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace blink::mojom {
enum class FetchCacheStorageSideDataWriterTag : int32_t;
}  // namespace blink::mojom

namespace network {

enum class CacheStorageSideDataWriterTag {
  kDefault,
};

namespace mojom {
enum class CacheStorageSideDataWriterTag : int32_t;
class CacheStorageSideDataWriter;
namespace blink {
class CacheStorageSideDataWriter;
}  // namespace blink
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
struct EnumTraits<blink::mojom::FetchCacheStorageSideDataWriterTag,
                  network::CacheStorageSideDataWriterTag> {
  static blink::mojom::FetchCacheStorageSideDataWriterTag ToMojom(
      network::CacheStorageSideDataWriterTag input) {
    return static_cast<blink::mojom::FetchCacheStorageSideDataWriterTag>(0);
  }

  static std::optional<network::CacheStorageSideDataWriterTag> FromMojom(
      blink::mojom::FetchCacheStorageSideDataWriterTag input) {
    return network::CacheStorageSideDataWriterTag::kDefault;
  }
};

template <typename T>
  requires(std::same_as<T, network::mojom::CacheStorageSideDataWriter> ||
           std::same_as<T, network::mojom::blink::CacheStorageSideDataWriter>)
struct CloneTraits<PendingRemote<T>> {
  static PendingRemote<T> Clone(const PendingRemote<T>& input) {
    return PendingRemote<T>();
  }
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CACHE_STORAGE_SIDE_DATA_WRITER_MOJOM_TRAITS_H_
