// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_mojom_traits.h"

#include "base/check.h"

#include "third_party/abseil-cpp/absl/types/variant.h"

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

// static
content::AttributionReport::EventLevelData::Id
UnionTraits<attribution_internals::mojom::ReportIDDataView,
            content::AttributionReport::Id>::
    event_level_id(const content::AttributionReport::Id& id) {
  return absl::get<content::AttributionReport::EventLevelData::Id>(id);
}

// static
content::AttributionReport::AggregatableAttributionData::Id
UnionTraits<attribution_internals::mojom::ReportIDDataView,
            content::AttributionReport::Id>::
    aggregatable_attribution_id(const content::AttributionReport::Id& id) {
  return absl::get<content::AttributionReport::AggregatableAttributionData::Id>(
      id);
}

// static
bool UnionTraits<attribution_internals::mojom::ReportIDDataView,
                 content::AttributionReport::Id>::
    Read(attribution_internals::mojom::ReportIDDataView data,
         content::AttributionReport::Id* out) {
  if (data.is_event_level_id()) {
    content::AttributionReport::EventLevelData::Id event_level_id;
    if (!data.ReadEventLevelId(&event_level_id))
      return false;
    *out = event_level_id;
    return true;
  } else if (data.is_aggregatable_attribution_id()) {
    content::AttributionReport::AggregatableAttributionData::Id
        aggregatable_attribution_id;
    if (!data.ReadAggregatableAttributionId(&aggregatable_attribution_id))
      return false;
    *out = aggregatable_attribution_id;
    return true;
  } else {
    return false;
  }
}

// static
attribution_internals::mojom::ReportIDDataView::Tag
UnionTraits<attribution_internals::mojom::ReportIDDataView,
            content::AttributionReport::Id>::
    GetTag(const content::AttributionReport::Id& id) {
  if (absl::holds_alternative<content::AttributionReport::EventLevelData::Id>(
          id)) {
    return attribution_internals::mojom::ReportIDDataView::Tag::EVENT_LEVEL_ID;
  } else {
    DCHECK(absl::holds_alternative<
           content::AttributionReport::AggregatableAttributionData::Id>(id));
    return attribution_internals::mojom::ReportIDDataView::Tag::
        AGGREGATABLE_ATTRIBUTION_ID;
  }
}

}  // namespace mojo
