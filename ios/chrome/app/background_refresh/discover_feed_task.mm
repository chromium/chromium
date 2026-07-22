// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/discover_feed_task.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/sequence_checker.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DiscoverFeedBackgroundRefreshOutcome {
  kSuccess = 0,
  kFailure = 1,
  kMaxValue = kFailure,
};
}  // namespace

@implementation DiscoverFeedTask {
  raw_ptr<DiscoverFeedService> _service;
  base::OnceClosure _completion;
  BOOL _refreshSuccess;
  BOOL _finished;
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithService:(DiscoverFeedService*)service {
  self = [super init];
  if (self) {
    _service = service;
    _refreshSuccess = NO;
    _finished = NO;
  }
  return self;
}

#pragma mark - AppRefreshProviderTask

- (void)executeWithCompletion:(base::OnceClosure)completion {
  DiscoverFeedService* service = _service.get();
  if (!service) {
    std::move(completion).Run();
    return;
  }

  _completion = std::move(completion);

  __weak __typeof(self) weakSelf = self;
  service->PerformBackgroundRefreshes(^(BOOL success) {
    [weakSelf handleRefreshCompletion:success];
  });
}

- (void)cancel {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_service) {
    _service->HandleBackgroundRefreshTaskExpiration();
  }
  [self handleRefreshCompletion:NO];
}

#pragma mark - Private

- (void)handleRefreshCompletion:(BOOL)success {
  base::OnceClosure completion;
  @synchronized(self) {
    if (_finished) {
      return;
    }
    _finished = YES;
    _refreshSuccess = success;
    completion = std::move(_completion);
  }

  if (completion) {
    std::move(completion).Run();
  }

  // Log metric after completion has been called/notified.
  DiscoverFeedBackgroundRefreshOutcome outcome =
      success ? DiscoverFeedBackgroundRefreshOutcome::kSuccess
              : DiscoverFeedBackgroundRefreshOutcome::kFailure;
  base::UmaHistogramEnumeration("IOS.Discover.BackgroundRefresh.Outcome",
                                outcome);
}

@end
