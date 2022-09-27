// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/storage/blink_storage_key_mojom_traits.h"

#include "base/types/optional_util.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom-blink.h"
#include "third_party/blink/renderer/platform/mojo/security_origin_mojom_traits.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site_mojom_traits.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::StorageKeyDataView, blink::BlinkStorageKey>::
    Read(blink::mojom::StorageKeyDataView data, blink::BlinkStorageKey* out) {
  scoped_refptr<const blink::SecurityOrigin> origin;
  if (!data.ReadOrigin(&origin))
    return false;
  DCHECK(origin);

  blink::BlinkSchemefulSite top_level_site;
  if (!data.ReadTopLevelSite(&top_level_site))
    return false;

  absl::optional<base::UnguessableToken> nonce;
  if (!data.ReadNonce(&nonce))
    return false;

  blink::mojom::blink::AncestorChainBit ancestor_chain_bit;
  if (!data.ReadAncestorChainBit(&ancestor_chain_bit))
    return false;

  *out = blink::BlinkStorageKey(std::move(origin), top_level_site,
                                base::OptionalToPtr(nonce), ancestor_chain_bit);
  return true;
}

}  // namespace mojo
