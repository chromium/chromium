// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/ad_display_size_mojom_traits.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"

#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

bool StructTraits<blink::mojom::AdSizeDataView, blink::AdSize>::Read(
    blink::mojom::AdSizeDataView data,
    blink::AdSize* out) {
  if (!data.ReadWidthUnits(&out->width_units) ||
      !data.ReadHeightUnits(&out->height_units)) {
    return false;
  }
  out->width = data.width();
  out->height = data.height();

  return blink::IsValidAdSize(*out);
}

bool StructTraits<blink::mojom::AdDescriptorDataView, blink::AdDescriptor>::
    Read(blink::mojom::AdDescriptorDataView data, blink::AdDescriptor* out) {
  if (!data.ReadUrl(&out->url) || !data.ReadSize(&out->size)) {
    return false;
  }

  return true;
}

}  // namespace mojo
