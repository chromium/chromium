// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/attribution_utils.h"

#include "base/notreached.h"
#include "services/network/public/mojom/attribution.mojom.h"

namespace network {

bool HasAttributionOsSupport(mojom::AttributionSupport attribution_support) {
  switch (attribution_support) {
    case mojom::AttributionSupport::kOs:
    case mojom::AttributionSupport::kWebAndOs:
      return true;
    case mojom::AttributionSupport::kWeb:
    case mojom::AttributionSupport::kNone:
      return false;
    case mojom::AttributionSupport::kUnset:
      NOTREACHED();
  }
}

bool HasAttributionWebSupport(mojom::AttributionSupport attribution_support) {
  switch (attribution_support) {
    case mojom::AttributionSupport::kWeb:
    case mojom::AttributionSupport::kWebAndOs:
      return true;
    case mojom::AttributionSupport::kOs:
    case mojom::AttributionSupport::kNone:
      return false;
    case mojom::AttributionSupport::kUnset:
      NOTREACHED();
  }
}

bool HasAttributionSupport(mojom::AttributionSupport attribution_support) {
  return HasAttributionWebSupport(attribution_support) ||
         HasAttributionOsSupport(attribution_support);
}

}  // namespace network
