// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_internals_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    aggregation_service_internals::mojom::AggregatableReportRequestIDDataView,
    content::AggregatableReportRequestStorageId>::
    Read(aggregation_service_internals::mojom::
             AggregatableReportRequestIDDataView data,
         content::AggregatableReportRequestStorageId* out) {
  *out = content::AggregatableReportRequestStorageId(data.value());
  return true;
}

}  // namespace mojo
