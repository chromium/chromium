// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_MOJOM_TRAITS_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_MOJOM_TRAITS_H_

#include <stdint.h>

#include "content/browser/aggregation_service/aggregatable_report_request_storage_id.h"
#include "content/browser/aggregation_service/aggregation_service_internals.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<
    aggregation_service_internals::mojom::AggregatableReportRequestIDDataView,
    content::AggregatableReportRequestStorageId> {
 public:
  static int64_t value(const content::AggregatableReportRequestStorageId& id) {
    return *id;
  }

  static bool Read(
      aggregation_service_internals::mojom::AggregatableReportRequestIDDataView
          data,
      content::AggregatableReportRequestStorageId* out);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_MOJOM_TRAITS_H_
