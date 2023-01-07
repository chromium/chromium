// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_MOJOM_TRAITS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_MOJOM_TRAITS_H_

#include <stdint.h>

#include "content/browser/attribution_reporting/attribution_internals.mojom.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"

namespace mojo {

template <>
class StructTraits<attribution_internals::mojom::EventLevelReportIDDataView,
                   content::AttributionReport::EventLevelData::Id> {
 public:
  static int64_t value(
      const content::AttributionReport::EventLevelData::Id& id) {
    return *id;
  }

  static bool Read(
      attribution_internals::mojom::EventLevelReportIDDataView data,
      content::AttributionReport::EventLevelData::Id* out);
};

template <>
class StructTraits<
    attribution_internals::mojom::AggregatableAttributionReportIDDataView,
    content::AttributionReport::AggregatableAttributionData::Id> {
 public:
  static int64_t value(
      const content::AttributionReport::AggregatableAttributionData::Id& id) {
    return *id;
  }

  static bool Read(
      attribution_internals::mojom::AggregatableAttributionReportIDDataView
          data,
      content::AttributionReport::AggregatableAttributionData::Id* out);
};

template <>
class UnionTraits<attribution_internals::mojom::ReportIDDataView,
                  content::AttributionReport::Id> {
 public:
  static content::AttributionReport::EventLevelData::Id event_level_id(
      const content::AttributionReport::Id& id);

  static content::AttributionReport::AggregatableAttributionData::Id
  aggregatable_attribution_id(const content::AttributionReport::Id& id);

  static bool Read(attribution_internals::mojom::ReportIDDataView data,
                   content::AttributionReport::Id* out);

  static attribution_internals::mojom::ReportIDDataView::Tag GetTag(
      const content::AttributionReport::Id& id);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_MOJOM_TRAITS_H_
