// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/blink_schemeful_site_mojom_traits.h"

#include "url/mojom/origin_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

// static
bool StructTraits<
    network::mojom::SchemefulSiteDataView,
    blink::BlinkSchemefulSite>::Read(network::mojom::SchemefulSiteDataView data,
                                     blink::BlinkSchemefulSite* out) {
  url::Origin site_as_origin;
  if (!data.ReadSiteAsOrigin(&site_as_origin))
    return false;

  return blink::BlinkSchemefulSite::FromWire(site_as_origin, out);
}

}  // namespace mojo
