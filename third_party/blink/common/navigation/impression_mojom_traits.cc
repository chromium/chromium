// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/impression_mojom_traits.h"

#include "build/build_config.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::ImpressionDataView, blink::Impression>::Read(
    blink::mojom::ImpressionDataView data,
    blink::Impression* out) {
  if (!data.ReadConversionDestination(&out->conversion_destination) ||
      !data.ReadReportingOrigin(&out->reporting_origin) ||
      !data.ReadExpiry(&out->expiry) ||
      !data.ReadAttributionSrcToken(&out->attribution_src_token))
    return false;

  out->impression_data = data.impression_data();
  out->priority = data.priority();
  return true;
}

}  // namespace mojo
