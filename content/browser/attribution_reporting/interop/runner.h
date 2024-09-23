// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_INTEROP_RUNNER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_INTEROP_RUNNER_H_

#include <string>
#include <string_view>

#include "base/types/expected.h"
#include "base/values.h"

class GURL;

namespace content {

struct AttributionInteropRun;
struct AttributionInteropOutput;

namespace aggregation_service {
class TestHpkeKey;
}  // namespace aggregation_service

base::expected<AttributionInteropOutput, std::string>
RunAttributionInteropSimulation(AttributionInteropRun,
                                const aggregation_service::TestHpkeKey&);

class ReportBodyAdjuster {
 public:
  virtual ~ReportBodyAdjuster() = default;

  virtual void AdjustAggregatable(base::Value::Dict&) {}

  virtual void AdjustEventLevel(base::Value::Dict&) {}

  // By default, calls `AdjustEventLevel()` if `debug_data_type` is expected to
  // have a full event-level-report body.
  virtual void AdjustVerboseDebug(std::string_view debug_data_type,
                                  base::Value::Dict& body);

  virtual void AdjustAggregatableDebug(base::Value::Dict&) {}
};

void MaybeAdjustReportBody(const GURL&,
                           base::Value& payload,
                           ReportBodyAdjuster&);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_INTEROP_RUNNER_H_
