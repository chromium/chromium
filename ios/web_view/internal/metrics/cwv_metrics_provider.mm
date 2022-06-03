// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/metrics/cwv_metrics_provider_internal.h"

#include "components/metrics/library_support/histogram_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVMetricsProvider {
  std::unique_ptr<metrics::HistogramManager> _histogramManager;
}

+ (CWVMetricsProvider*)sharedInstance {
  static CWVMetricsProvider* provider;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    auto histogramManager = std::make_unique<metrics::HistogramManager>();
    provider = [[CWVMetricsProvider alloc]
        initWithHistogramManager:std::move(histogramManager)];
  });
  return provider;
}

- (instancetype)initWithHistogramManager:
    (std::unique_ptr<metrics::HistogramManager>)histogramManager {
  self = [super init];
  if (self) {
    _histogramManager = std::move(histogramManager);
  }
  return self;
}

#pragma mark - Public

- (NSData*)consumeMetrics {
  std::vector<uint8_t> deltas;
  _histogramManager->GetDeltas(&deltas);
  return [NSData dataWithBytes:deltas.data() length:deltas.size()];
}

@end
