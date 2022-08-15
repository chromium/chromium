// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/private_aggregation_features.h"

#include "base/feature_list.h"

namespace content {

const base::Feature kPrivateAggregationApi = {
    "PrivateAggregationApi", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace content
