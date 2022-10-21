// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_UTILS_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_UTILS_H_

#include <string>

#include "content/browser/private_aggregation/private_aggregation_budget_key.h"

namespace content::private_aggregation {

std::string GetReportingPath(PrivateAggregationBudgetKey::Api api,
                             bool is_immediate_debug_report);

std::string GetApiIdentifier(PrivateAggregationBudgetKey::Api api);

}  // namespace content::private_aggregation

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_UTILS_H_
