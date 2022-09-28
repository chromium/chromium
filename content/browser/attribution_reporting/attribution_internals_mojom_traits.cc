// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_mojom_traits.h"

#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mojo {

namespace {
using ::attribution_internals::mojom::ReportIDDataView;
using ::content::AttributionReport;
}  // namespace

// static
bool StructTraits<attribution_internals::mojom::EventLevelReportIDDataView,
                  AttributionReport::EventLevelData::Id>::
    Read(attribution_internals::mojom::EventLevelReportIDDataView data,
         AttributionReport::EventLevelData::Id* out) {
  *out = AttributionReport::EventLevelData::Id(data.value());
  return true;
}

// static
bool StructTraits<
    attribution_internals::mojom::AggregatableAttributionReportIDDataView,
    AttributionReport::AggregatableAttributionData::Id>::
    Read(attribution_internals::mojom::AggregatableAttributionReportIDDataView
             data,
         AttributionReport::AggregatableAttributionData::Id* out) {
  *out = AttributionReport::AggregatableAttributionData::Id(data.value());
  return true;
}

// static
AttributionReport::EventLevelData::Id
UnionTraits<ReportIDDataView, AttributionReport::Id>::event_level_id(
    const AttributionReport::Id& id) {
  return absl::get<AttributionReport::EventLevelData::Id>(id);
}

// static
AttributionReport::AggregatableAttributionData::Id
UnionTraits<ReportIDDataView, AttributionReport::Id>::
    aggregatable_attribution_id(const AttributionReport::Id& id) {
  return absl::get<AttributionReport::AggregatableAttributionData::Id>(id);
}

// static
bool UnionTraits<ReportIDDataView, AttributionReport::Id>::Read(
    ReportIDDataView data,
    AttributionReport::Id* out) {
  switch (data.tag()) {
    case ReportIDDataView::Tag::kEventLevelId: {
      AttributionReport::EventLevelData::Id event_level_id;
      if (!data.ReadEventLevelId(&event_level_id))
        return false;
      *out = event_level_id;
      return true;
    }
    case ReportIDDataView::Tag::kAggregatableAttributionId: {
      AttributionReport::AggregatableAttributionData::Id
          aggregatable_attribution_id;
      if (!data.ReadAggregatableAttributionId(&aggregatable_attribution_id))
        return false;
      *out = aggregatable_attribution_id;
      return true;
    }
  }
}

// static
ReportIDDataView::Tag
UnionTraits<ReportIDDataView, AttributionReport::Id>::GetTag(
    const AttributionReport::Id& id) {
  switch (AttributionReport::GetReportType(id)) {
    case AttributionReport::Type::kEventLevel:
      return ReportIDDataView::Tag::kEventLevelId;
    case AttributionReport::Type::kAggregatableAttribution:
      return ReportIDDataView::Tag::kAggregatableAttributionId;
  }
}

}  // namespace mojo
