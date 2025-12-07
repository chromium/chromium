// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_
#define CONTENT_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_

#include <stdint.h>

#include "content/common/content_export.h"

namespace content {

// Bucketize `sample` into 50 buckets, capped at maximum and distributed
// non-linearly similarly to base::Histogram::InitializeBucketRanges.
CONTENT_EXPORT int32_t Bucketize3PCDHeuristicSample(int64_t sample,
                                                    int64_t maximum);

}  // namespace content

#endif  // CONTENT_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_
