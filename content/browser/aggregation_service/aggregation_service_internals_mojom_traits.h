// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_MOJOM_TRAITS_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_MOJOM_TRAITS_H_

#include <stdint.h>

#include "content/browser/aggregation_service/aggregation_service_internals.mojom.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<
    aggregation_service_internals::mojom::AggregatableReportRequestIDDataView,
    content::AggregationServiceStorage::RequestId> {
 public:
  static int64_t value(
      const content::AggregationServiceStorage::RequestId& id) {
    return *id;
  }

  static bool Read(
      aggregation_service_internals::mojom::AggregatableReportRequestIDDataView
          data,
      content::AggregationServiceStorage::RequestId* out);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_MOJOM_TRAITS_H_
