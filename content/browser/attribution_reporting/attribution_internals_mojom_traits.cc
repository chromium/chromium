// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<attribution_internals::mojom::EventLevelReportIDDataView,
                  content::AttributionReport::EventLevelData::Id>::
    Read(attribution_internals::mojom::EventLevelReportIDDataView data,
         content::AttributionReport::EventLevelData::Id* out) {
  *out = content::AttributionReport::EventLevelData::Id(data.value());
  return true;
}

// static
bool StructTraits<
    attribution_internals::mojom::AggregatableAttributionReportIDDataView,
    content::AttributionReport::AggregatableAttributionData::Id>::
    Read(attribution_internals::mojom::AggregatableAttributionReportIDDataView
             data,
         content::AttributionReport::AggregatableAttributionData::Id* out) {
  *out =
      content::AttributionReport::AggregatableAttributionData::Id(data.value());
  return true;
}

}  // namespace mojo
