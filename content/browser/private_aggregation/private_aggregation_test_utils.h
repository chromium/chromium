// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_

#include <vector>

#include "base/callback_forward.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/common/aggregatable_report.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace url {
class Origin;
}

namespace content {

class MockPrivateAggregationBudgeter : public PrivateAggregationBudgeter {
 public:
  MockPrivateAggregationBudgeter();
  ~MockPrivateAggregationBudgeter() override;

  MOCK_METHOD(void,
              ConsumeBudget,
              (int,
               const PrivateAggregationBudgetKey&,
               base::OnceCallback<void(bool)>),
              (override));
};

class MockPrivateAggregationHost : public PrivateAggregationHost {
 public:
  MockPrivateAggregationHost();
  ~MockPrivateAggregationHost() override;

  MOCK_METHOD(bool,
              BindNewReceiver,
              (url::Origin,
               PrivateAggregationBudgetKey::Api,
               mojo::PendingReceiver<mojom::PrivateAggregationHost>),
              (override));

  MOCK_METHOD(void,
              SendHistogramReport,
              (std::vector<mojom::AggregatableReportHistogramContributionPtr>,
               mojom::AggregationServiceMode aggregation_mode),
              (override));
};

bool operator==(const PrivateAggregationBudgetKey::TimeWindow&,
                const PrivateAggregationBudgetKey::TimeWindow&);

bool operator==(const PrivateAggregationBudgetKey&,
                const PrivateAggregationBudgetKey&);

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_
