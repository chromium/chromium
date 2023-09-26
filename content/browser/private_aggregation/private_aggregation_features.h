// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_FEATURES_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_FEATURES_H_

#include "base/feature_list.h"
#include "content/common/content_export.h"

namespace content {

// Enables a set of bundled Private Aggregation API enhancements. Specifically:
// - Controls whether third-party cookie eligibility should be queried before
//   allowing debug mode to be used by a context. If enabled, any
//   `enableDebugMode()` calls in a context that does not have third-party
//   cookie eligibility will essentially have no effect. This feature has no
//   effect on debug mode if
//   `blink::features::kPrivateAggregationApiDebugModeEnabledAtAll` is disabled.
// - Whether to send a null report if a context ID is specified.
// - Whether to reduce the delay for reports if a context ID is specified.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateAggregationApiBundledEnhancements);

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_FEATURES_H_
