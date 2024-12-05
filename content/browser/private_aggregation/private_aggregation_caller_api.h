// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_CALLER_API_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_CALLER_API_H_

#include <string_view>

#include "base/notreached.h"

namespace content {

enum class PrivateAggregationCallerApi { kProtectedAudience, kSharedStorage };

constexpr std::string_view PrivateAggregationCallerApiToString(
    PrivateAggregationCallerApi api) {
  switch (api) {
    case PrivateAggregationCallerApi::kProtectedAudience:
      return "kProtectedAudience";
    case PrivateAggregationCallerApi::kSharedStorage:
      return "kSharedStorage";
  }
  NOTREACHED();
}
}

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_CALLER_API_H_
