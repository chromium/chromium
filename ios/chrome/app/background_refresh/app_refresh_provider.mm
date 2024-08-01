// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/app_refresh_provider.h"

#import "base/notreached.h"

@interface AppRefreshProvider ()

// Key storing the last run time.
@property(nonatomic, readonly) NSString* defaultsKey;

@end

@implementation AppRefreshProvider

- (instancetype)init {
  if (self = [super init]) {
    // TODO(crbug.com/354918222): Use this value (perhaps a longer interval) for
    // scheduling refreshes.
    _refreshInterval = base::Minutes(15);
  }
  return self;
}

- (base::Time)lastRun {
  NSDate* lastRunDate =
      [[NSUserDefaults standardUserDefaults] objectForKey:self.defaultsKey];
  // If the provider was never run before, return a time in the distant past.
  base::Time lastRunTime = lastRunDate
                               ? base::Time::FromNSDate(lastRunDate)
                               : base::Time::FromNSDate([NSDate distantPast]);
  return lastRunTime;
}

- (void)setLastRun:(base::Time)lastRun {
  // Store the last run time.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:lastRun.ToNSDate() forKey:self.defaultsKey];
}

- (BOOL)isDue {
  return base::Time::Now() - self.lastRun > self.refreshInterval;
}

- (NSString*)defaultsKey {
  CHECK(self.identifier.length > 0)
      << "Subclasses of AppRefreshProvider must provide an identifier.";
  return
      [@"AppRefreshProvider_lastRun_" stringByAppendingString:self.identifier];
}

- (void)handleRefreshWithCompletion:(ProceduralBlock)completion {
  NOTREACHED() << "Subclasses of AppRefreshProvider must implement "
                  "handleRefreshWithCompletion:";
}

// Terminate the running task immediately.
- (void)cancelRefresh {
  // TODO(crbug.com/354918188): Implement cancellation.
}

@end
