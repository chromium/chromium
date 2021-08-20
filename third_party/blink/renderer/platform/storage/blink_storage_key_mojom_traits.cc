// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/storage/blink_storage_key_mojom_traits.h"

#include "base/unguessable_token.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
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

  if (nonce.has_value()) {
    // This call does not need or use the top_level_site. But since the
    // implementation sets the top_level_site to be same-site with the origin we
    // should confirm that's still the case.
    //
    // TODO(crbug.com/1199077): This should be represented by a union since the
    // usages of top_level_site and nonce are mutally exclusive.
    if (top_level_site != blink::BlinkSchemefulSite(origin))
      return false;

    *out = blink::BlinkStorageKey::CreateWithNonce(std::move(origin),
                                                   nonce.value());
  } else {
    *out = blink::BlinkStorageKey(std::move(origin), top_level_site);
  }

  return true;
}

}  // namespace mojo
