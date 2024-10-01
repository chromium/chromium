// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/app_refresh_provider.h"

#import "base/cancelable_callback.h"
#import "base/functional/bind.h"
#import "base/notreached.h"
#import "base/task/thread_pool.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

@interface AppRefreshProvider ()

// Key storing the last run time.
@property(nonatomic, readonly) NSString* defaultsKey;

@end

@implementation AppRefreshProvider {
  base::CancelableOnceCallback<void()> _task;
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)init {
  if ((self = [super init])) {
    // TODO(crbug.com/354918222): Use this value (perhaps a longer interval) for
    // scheduling refreshes.
    _refreshInterval = base::Minutes(15);
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
  id<AppRefreshProviderTask> task = [self task];
  _task.Reset(base::BindOnce(^{
    [task execute];
  }));

  __weak __typeof(self) weakSelf = self;
  base::OnceCallback callback = base::BindOnce(^{
    [weakSelf refreshTaskFinishedWithCompletion:completion];
  });

  // `_task` has a cancelable wrapper, so `_task.callback()` is the underlying
  // OnceCallback to execute.
  self.taskThread->PostTaskAndReply(FROM_HERE, _task.callback(),
                                    std::move(callback));
}

// Terminate the running task immediately.
- (void)cancelRefresh {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  _task.Cancel();
}

- (id<AppRefreshProviderTask>)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NOTREACHED() << "Subclasses of AppRefreshProvider must implement -task";
}

#pragma mark - Private methods

- (void)refreshTaskFinishedWithCompletion:(ProceduralBlock)completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  self.lastRun = base::Time::Now();

  completion();
}

@end
