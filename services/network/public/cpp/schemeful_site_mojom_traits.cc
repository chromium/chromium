// Copyright 2020 The Chromium Authors. All rights reserved.
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

  // The origin passed into this constructor may not match the
  // `site_as_origin_` used as the internal representation of the schemeful
  // site. However, a valid SchemefulSite's internal origin should result in a
  // match if used to construct another SchemefulSite. Thus, if there is a
  // mismatch here, we must indicate a failure.
  net::SchemefulSite ss(site_as_origin);
  bool success = site_as_origin == ss.site_as_origin_;
  if (success)
    *out = std::move(ss);
  return success;
}

}  // namespace mojo
