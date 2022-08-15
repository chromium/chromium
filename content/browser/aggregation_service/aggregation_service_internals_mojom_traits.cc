// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_internals_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    aggregation_service_internals::mojom::AggregatableReportRequestIDDataView,
    content::AggregationServiceStorage::RequestId>::
    Read(aggregation_service_internals::mojom::
             AggregatableReportRequestIDDataView data,
         content::AggregationServiceStorage::RequestId* out) {
  *out = content::AggregationServiceStorage::RequestId(data.value());
  return true;
}

}  // namespace mojo
