// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/schemeful_site_mojom_traits.h"

#include "url/origin.h"

namespace mojo {

bool StructTraits<network::mojom::SchemefulSiteDataView, net::SchemefulSite>::
    Read(network::mojom::SchemefulSiteDataView data, net::SchemefulSite* out) {
  url::Origin site_as_origin;
  if (!data.ReadSiteAsOrigin(&site_as_origin))
    return false;

  return net::SchemefulSite::FromWire(site_as_origin, out);
}

}  // namespace mojo
