// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_H_

#include "base/callback_forward.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class AggregatableReport;
class AggregatableReportRequest;
class BrowserContext;

// External interface for the aggregation service.
class AggregationService {
 public:
  using AssemblyStatus = AggregatableReportAssembler::AssemblyStatus;
  using AssemblyCallback = AggregatableReportAssembler::AssemblyCallback;

  virtual ~AggregationService() = default;

  // Gets the AggregationService that should be used for handling aggregations
  // in the given `browser_context`. Returns nullptr if aggregation service is
  // not enabled.
  static AggregationService* GetService(BrowserContext* browser_context);

  // Construct an AggregatableReport from the information in `report_request`.
  // `callback` will  be run once completed which returns the assembled report
  // if successful, otherwise `absl::nullopt` will be returned.
  virtual void AssembleReport(AggregatableReportRequest report_request,
                              AssemblyCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_H_
