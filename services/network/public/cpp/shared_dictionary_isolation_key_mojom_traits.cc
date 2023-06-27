// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_dictionary_isolation_key_mojom_traits.h"

#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "url/mojom/origin_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::SharedDictionaryIsolationKeyDataView,
                  net::SharedDictionaryIsolationKey>::
    Read(network::mojom::SharedDictionaryIsolationKeyDataView data,
         net::SharedDictionaryIsolationKey* out) {
  url::Origin frame_origin;
  if (!data.ReadFrameOrigin(&frame_origin)) {
    return false;
  }
  if (frame_origin.opaque()) {
    return false;
  }

  net::SchemefulSite top_frame_site;
  if (!data.ReadTopFrameSite(&top_frame_site)) {
    return false;
  }
  if (top_frame_site.opaque()) {
    return false;
  }

  *out = net::SharedDictionaryIsolationKey(frame_origin, top_frame_site);
  return true;
}

}  // namespace mojo
