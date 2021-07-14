// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/storage_key/storage_key.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

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

  static const absl::optional<base::UnguessableToken>& nonce(
      const blink::StorageKey& key) {
    return key.nonce();
  }

  static bool Read(blink::mojom::StorageKeyDataView data,
                   blink::StorageKey* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_MOJOM_TRAITS_H_
