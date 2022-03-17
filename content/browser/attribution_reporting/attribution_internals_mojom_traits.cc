// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<content::mojom::AttributionReportEventLevelIDDataView,
                  content::AttributionReport::EventLevelData::Id>::
    Read(content::mojom::AttributionReportEventLevelIDDataView data,
         content::AttributionReport::EventLevelData::Id* out) {
  *out = content::AttributionReport::EventLevelData::Id(data.value());
  return true;
}

// static
bool StructTraits<
    content::mojom::AttributionReportAggregatableAttributionIDDataView,
    content::AttributionReport::AggregatableAttributionData::Id>::
    Read(
        content::mojom::AttributionReportAggregatableAttributionIDDataView data,
        content::AttributionReport::AggregatableAttributionData::Id* out) {
  *out =
      content::AttributionReport::AggregatableAttributionData::Id(data.value());
  return true;
}

}  // namespace mojo
