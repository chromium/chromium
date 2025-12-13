// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/app_refresh_provider.h"

#import "base/functional/bind.h"
#import "base/notreached.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/app/background_refresh/background_refresh_metrics.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

@interface AppRefreshProvider ()

// Key storing the last run time.
@property(nonatomic, readonly) NSString* defaultsKey;

// Read/write override for the due property.
@property(nonatomic, readwrite, getter=isDue) BOOL due;

@end

@implementation AppRefreshProvider {
  SEQUENCE_CHECKER(_sequenceChecker);
  base::Time _startTime;
  BOOL _isCancelled;
}

- (instancetype)init {
  if ((self = [super init])) {
    _refreshInterval = base::Minutes(30);
  }
  return self;
}

#pragma mark - Public properties

- (base::Time)lastRun {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  NSDate* lastRunDate =
      [[NSUserDefaults standardUserDefaults] objectForKey:self.defaultsKey];
  // If the provider was never run before, return a time in the distant past.
  base::Time lastRunTime = lastRunDate
                               ? base::Time::FromNSDate(lastRunDate)
                               : base::Time::FromNSDate([NSDate distantPast]);
  return lastRunTime;
}

- (void)setLastRun:(base::Time)lastRun {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // Store the last run time.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:lastRun.ToNSDate() forKey:self.defaultsKey];
}

- (BOOL)isDue {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  return base::Time::Now() - self.lastRun > self.refreshInterval;
}

- (scoped_refptr<base::SingleThreadTaskRunner>)taskThread {
  return web::GetIOThreadTaskRunner({});
}

#pragma mark - Private properties

- (NSString*)defaultsKey {
  CHECK(self.identifier.length > 0)
      << "Subclasses of AppRefreshProvider must provide an identifier.";
  return
      [@"AppRefreshProvider_lastRun_" stringByAppendingString:self.identifier];
}

#pragma mark - Public methods

// Called on the main thread, runs tasks on (by default) the IO thread.
- (void)handleRefreshWithCompletion:(ProceduralBlock)completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _startTime = base::Time::Now();
  _isCancelled = NO;

  id<AppRefreshProviderTask> task = [self task];

  // Create a weak pointer to `self` to be used in the reply callback.
  // The reply callback runs on the main thread.
  __weak AppRefreshProvider* weakSelf = self;

  base::OnceClosure taskClosure = base::BindOnce(^{
    [task execute];
  });

  base::OnceClosure replyClosure = base::BindOnce(^{
    if (weakSelf) {
      [weakSelf refreshTaskFinishedWithCompletion:completion];
    }
  });

  self.taskThread->PostTaskAndReply(FROM_HERE, std::move(taskClosure),
                                    std::move(replyClosure));
}

// Cancel the task, so it won't run if it hasn't started.
- (void)cancelRefresh {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isCancelled = YES;
}

- (id<AppRefreshProviderTask>)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NOTREACHED() << "Subclasses of AppRefreshProvider must implement -task";
}

#pragma mark - Private methods

- (void)refreshTaskFinishedWithCompletion:(ProceduralBlock)completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // If cancelled, do nothing.
  if (_isCancelled) {
    return;
  }

  // If the metric is missing from the dashboard, you need to add the provider
  // identifier to the tokens for the histogram
  // IOS.BackgroundRefresh.Provider.Duration.{ProviderID} in
  // tools/metrics/histograms/metadata/ios/histograms.xml.
  RecordProviderExecutionDuration(self.identifier,
                                  base::Time::Now() - _startTime);
  self.lastRun = base::Time::Now();

  completion();
}

@end
