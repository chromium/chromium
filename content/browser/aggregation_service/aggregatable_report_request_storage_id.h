// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_REQUEST_STORAGE_ID_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_REQUEST_STORAGE_ID_H_

#include <stdint.h>

#include "base/types/strong_alias.h"

namespace content {

class AggregatableReportRequest;

// Defined here instead of in `AggregationServiceStorage` to allow mojo traits
// to depend on only this type.
using AggregatableReportRequestStorageId =
    base::StrongAlias<AggregatableReportRequest, int64_t>;

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_REQUEST_STORAGE_ID_H_
