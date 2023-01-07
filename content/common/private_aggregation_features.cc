// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/private_aggregation_features.h"

#include "base/feature_list.h"

namespace content {

BASE_FEATURE(kPrivateAggregationApi,
             "PrivateAggregationApi",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace content
