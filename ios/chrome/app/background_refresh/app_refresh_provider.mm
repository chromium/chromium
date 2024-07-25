// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/app_refresh_provider.h"

#import "base/notreached.h"

@interface AppRefreshProvider ()

@property(nonatomic, readonly) NSString* defaultsKey;

@end

@implementation AppRefreshProvider {
  base::TimeDelta _refreshInterval;
}

- (instancetype)init {
  if (self = [super init]) {
    // TODO(crbug.com/354918222): Use this value (perhaps a longer interval) for
    // scheduling refreshes.
    _refreshInterval = base::Minutes(15);
  }
  return self;
}

- (base::Time)lastRun {
  // TODO(crbug.com/354918222): Support correctly storing and tracking the last
  // run time.
  return base::Time::FromNSDate([NSDate distantPast]);
}

- (void)setLastRun:(base::Time)lastRun {
  // TODO(crbug.com/354918222): Support correctly storing and tracking the last
  // run time. No-op for now, should store this in local storage (on the main
  // thread).
}

- (BOOL)isDue {
  return base::Time::Now() - self.lastRun < self.refreshInterval;
}

- (NSString*)defaultsKey {
  // TODO(crbug.com/354918222): Determine whether to use user defaults (for
  // thread safety) or local storage (for compatibility) for storing this key.
  // Returning an NSString assumes user deafults, but that's changeable.
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
