// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_MOJOM_TRAITS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_MOJOM_TRAITS_H_

#include <stdint.h>

#include "content/browser/attribution_reporting/attribution_internals.mojom.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class EnumTraits<content::mojom::AttributionSourceType,
                 content::AttributionSourceType> {
 public:
  static content::mojom::AttributionSourceType ToMojom(
      content::AttributionSourceType input) {
    switch (input) {
      case content::AttributionSourceType::kNavigation:
        return content::mojom::AttributionSourceType::kNavigation;
      case content::AttributionSourceType::kEvent:
        return content::mojom::AttributionSourceType::kEvent;
    }
  }

  static bool FromMojom(content::mojom::AttributionSourceType input,
                        content::AttributionSourceType* out) {
    switch (input) {
      case content::mojom::AttributionSourceType::kNavigation:
        *out = content::AttributionSourceType::kNavigation;
        break;
      case content::mojom::AttributionSourceType::kEvent:
        *out = content::AttributionSourceType::kEvent;
        break;
    }

    return true;
  }
};

template <>
class StructTraits<content::mojom::AttributionReportIDDataView,
                   content::AttributionReport::EventLevelData::Id> {
 public:
  static int64_t value(
      const content::AttributionReport::EventLevelData::Id& id) {
    return *id;
  }

  static bool Read(content::mojom::AttributionReportIDDataView data,
                   content::AttributionReport::EventLevelData::Id* out);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_MOJOM_TRAITS_H_
