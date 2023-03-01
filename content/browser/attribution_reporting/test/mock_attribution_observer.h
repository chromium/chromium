// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_OBSERVER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_OBSERVER_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class MockAttributionObserver : public AttributionObserver {
 public:
  MockAttributionObserver();
  ~MockAttributionObserver() override;

  MockAttributionObserver(const MockAttributionObserver&) = delete;
  MockAttributionObserver(MockAttributionObserver&&) = delete;

  MockAttributionObserver& operator=(const MockAttributionObserver&) = delete;
  MockAttributionObserver& operator=(MockAttributionObserver&&) = delete;

  MOCK_METHOD(void, OnSourcesChanged, (), (override));

  MOCK_METHOD(void, OnReportsChanged, (), (override));

  MOCK_METHOD(void,
              OnSourceHandled,
              (const StorableSource&,
               absl::optional<uint64_t> cleared_debug_key,
               StorableSource::Result),
              (override));

  MOCK_METHOD(void,
              OnReportSent,
              (const AttributionReport&,
               bool is_debug_report,
               const SendResult&),
              (override));

  MOCK_METHOD(void,
              OnDebugReportSent,
              (const AttributionDebugReport&, int status, base::Time),
              (override));

  MOCK_METHOD(void,
              OnTriggerHandled,
              (const AttributionTrigger&,
               absl::optional<uint64_t> cleared_debug_key,
               const CreateReportResult&),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_OBSERVER_H_
