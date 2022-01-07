// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_MOJOM_TRAITS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_MOJOM_TRAITS_H_

#include <stdint.h>

#include "content/browser/attribution_reporting/attribution_internals.mojom.h"
#include "content/browser/attribution_reporting/event_attribution_report.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class EnumTraits<content::mojom::SourceType,
                 content::StorableSource::SourceType> {
 public:
  static content::mojom::SourceType ToMojom(
      content::StorableSource::SourceType input) {
    switch (input) {
      case content::StorableSource::SourceType::kNavigation:
        return content::mojom::SourceType::kNavigation;
      case content::StorableSource::SourceType::kEvent:
        return content::mojom::SourceType::kEvent;
    }
  }

  static bool FromMojom(content::mojom::SourceType input,
                        content::StorableSource::SourceType* out) {
    switch (input) {
      case content::mojom::SourceType::kNavigation:
        *out = content::StorableSource::SourceType::kNavigation;
        break;
      case content::mojom::SourceType::kEvent:
        *out = content::StorableSource::SourceType::kEvent;
        break;
    }

    return true;
  }
};

template <>
class StructTraits<content::mojom::AttributionReportIDDataView,
                   content::EventAttributionReport::Id> {
 public:
  static int64_t value(const content::EventAttributionReport::Id& id) {
    return *id;
  }

  static bool Read(content::mojom::AttributionReportIDDataView data,
                   content::EventAttributionReport::Id* out);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_MOJOM_TRAITS_H_
