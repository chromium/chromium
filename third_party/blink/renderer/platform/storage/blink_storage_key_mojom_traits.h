// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_MOJOM_TRAITS_H_

#include "third_party/blink/public/mojom/storage_key/storage_key.mojom-shared.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT
    StructTraits<blink::mojom::StorageKeyDataView, blink::BlinkStorageKey> {
  static const scoped_refptr<const blink::SecurityOrigin>& origin(
      const blink::BlinkStorageKey& input) {
    return input.GetSecurityOrigin();
  }

  static const absl::optional<base::UnguessableToken>& nonce(
      const blink::BlinkStorageKey& input) {
    return input.GetNonce();
  }

  static bool Read(blink::mojom::StorageKeyDataView data,
                   blink::BlinkStorageKey* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_MOJOM_TRAITS_H_
