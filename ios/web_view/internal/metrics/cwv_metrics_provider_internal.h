// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_METRICS_CWV_METRICS_PROVIDER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_METRICS_CWV_METRICS_PROVIDER_INTERNAL_H_

#import "ios/web_view/public/cwv_metrics_provider.h"

#include <memory>

NS_ASSUME_NONNULL_BEGIN

namespace metrics {
class HistogramManager;
}  // namespace metrics

@interface CWVMetricsProvider ()

// Initializes with the wrapped |histogramManager|.
- (instancetype)initWithHistogramManager:
    (std::unique_ptr<metrics::HistogramManager>)histogramManager;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_METRICS_CWV_METRICS_PROVIDER_INTERNAL_H_
