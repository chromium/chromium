// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.h"
#include "third_party/blink/public/mojom/storage_key/storage_key.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace net {
class SchemefulSite;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace mojo {

template <>
class BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::StorageKeyDataView, blink::StorageKey> {
 public:
  static const url::Origin& origin(const blink::StorageKey& key) {
    return key.origin();
  }

  // TODO(crbug.com/1159586): Return by reference when internal copy is removed.
  static const net::SchemefulSite top_level_site(const blink::StorageKey& key) {
    // We use `CopyWithForceEnabledThirdPartyStoragePartitioning` to ensure the
    // partitioned values are preserved. The constructor on the other side will
    // properly restore `top_level_site_` as derived from origin_ if
    // `kThirdPartyStoragePartitioning` is disabled.
    return key.CopyWithForceEnabledThirdPartyStoragePartitioning()
        .top_level_site();
  }

  static const absl::optional<base::UnguessableToken>& nonce(
      const blink::StorageKey& key) {
    return key.nonce();
  }

  static blink::mojom::AncestorChainBit ancestor_chain_bit(
      const blink::StorageKey& key) {
    // We use `CopyWithForceEnabledThirdPartyStoragePartitioning` to ensure the
    // partitioned values are preserved. The constructor on the other side will
    // properly restore `ancestor_chain_bit_`  to be `kSameSite` if
    // `kThirdPartyStoragePartitioning` is disabled.
    return key.CopyWithForceEnabledThirdPartyStoragePartitioning()
        .ancestor_chain_bit();
  }

  static bool Read(blink::mojom::StorageKeyDataView data,
                   blink::StorageKey* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_MOJOM_TRAITS_H_
