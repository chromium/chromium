// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_

#include "content/browser/private_aggregation/private_aggregation_budget_key.h"

namespace content {

bool operator==(const PrivateAggregationBudgetKey::TimeWindow&,
                const PrivateAggregationBudgetKey::TimeWindow&);

bool operator==(const PrivateAggregationBudgetKey&,
                const PrivateAggregationBudgetKey&);

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_
