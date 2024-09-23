// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_REFRESH_APP_REFRESH_PROVIDER_H_
#define IOS_CHROME_APP_BACKGROUND_REFRESH_APP_REFRESH_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "base/time/time.h"

// Superclass for classes that implement background refresh tasks. Each feature
// that uses background refresh to update data should implement its own
// AppRefreshProvider subclass, and add those to the background refresh app
// agent by calling `[BackgroundRefreshAppAgent addAppRefreshProvider:]`.
@interface AppRefreshProvider : NSObject

// An identifier for the provider. This is used to index values in user
// defaults, so it shouldn't change. Subclasses should return a constant value
// for this.
@property(nonatomic, readonly) NSString* identifier;

// Refresh interval for this provider. Default is 15 minutes.
@property(nonatomic, readonly) base::TimeDelta refreshInterval;

// Last *completed* run time for the provider's operations. Backed by a user
// default value.
@property(nonatomic) base::Time lastRun;

// YES if the provider is due (hasn't run since the last interval).
@property(nonatomic, readonly, getter=isDue) BOOL due;

// Handle the refresh task and call `completion` back on the initial thread.
// Template method -- subclasses must implement this, and should never call the
// superclass implementation.
- (void)handleRefreshWithCompletion:(ProceduralBlock)completion;

// TODO(crbug.com/354918188): Implement cancellation.
// Terminate the running task immediately.
- (void)cancelRefresh;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_REFRESH_APP_REFRESH_PROVIDER_H_
