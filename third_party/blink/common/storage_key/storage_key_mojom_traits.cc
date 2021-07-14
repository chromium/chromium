// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"
#include "url/mojom/origin_mojom_traits.h"

#include "url/origin.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::StorageKeyDataView, blink::StorageKey>::Read(
    blink::mojom::StorageKeyDataView data,
    blink::StorageKey* out) {
  url::Origin origin;
  if (!data.ReadOrigin(&origin))
    return false;

  absl::optional<base::UnguessableToken> nonce;
  if (!data.ReadNonce(&nonce))
    return false;

  if (nonce.has_value())
    *out = blink::StorageKey::CreateWithNonce(origin, *nonce);
  else
    *out = blink::StorageKey(origin);
  return true;
}

}  // namespace mojo
