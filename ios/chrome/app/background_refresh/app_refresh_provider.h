// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_REFRESH_APP_REFRESH_PROVIDER_H_
#define IOS_CHROME_APP_BACKGROUND_REFRESH_APP_REFRESH_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "base/ios/block_types.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"

// Protocol for objects which handle provider tasks. This is separate from the
// refresh provider so the task object can have a separate sequence checker.
@protocol AppRefreshProviderTask <NSObject>

@optional
// Do the work for the refresh task. This method must block and only return when
// the task is completed.
- (void)execute;

// Do the work for the refresh task asynchronously, invoking `completion` on the
// same sequence when finished.
- (void)executeWithCompletion:(base::OnceClosure)completion;

@end

// Superclass for classes that implement background refresh tasks. Each feature
// that uses background refresh to update data should implement its own
// AppRefreshProvider subclass, and add those to the background refresh app
// agent by calling `[BackgroundRefreshAppAgent addAppRefreshProvider:]`.
// Instances of this class are expected to do all work on the main thread
// except for the `-task` method, which will be called on the task runner
// configured in the `taskRunner` property.
@interface AppRefreshProvider : NSObject

// An identifier for the provider. This is used to index values in user
// defaults, so it shouldn't change. Subclasses should return a constant value
// for this.
@property(nonatomic, readonly) NSString* identifier;

// Refresh interval for this provider. Default is 30 minutes.
@property(nonatomic, readonly) base::TimeDelta refreshInterval;

// Last *completed* run time for the provider's operations. Backed by a user
// default value. This is not updated if the provider's task is canceled.
@property(nonatomic) base::Time lastRun;

// YES if the provider is due (hasn't run since the last interval).
@property(nonatomic, readonly, getter=isDue) BOOL due;

// Task runner to run `-task` on. Defaults to a sequenced task runner in the
// ThreadPool.
@property(nonatomic, readonly) scoped_refptr<base::SequencedTaskRunner>
    taskRunner;

// Handle the refresh task and call `completion` back on the main thread,
// updating the `lastRun` value.
- (void)handleRefreshWithCompletion:(ProceduralBlock)completion;

// Terminate the running task immediately. The completion block passed in via
// `-handleRefreshWithCompletion:` is not called, and the `lastRun` value is
// not updated.
- (void)cancelRefresh;

// Called on the main thread when the refresh task completes (or is cancelled).
// Subclasses can override this to perform cleanup.
- (void)refreshDidComplete;

// Return an object that will actually handle the task. The returned object's
// `execute` method will be called on `taskRunner`, and the whole implementation
// of that object should be sequence-affine for that task runner.
- (id<AppRefreshProviderTask>)task;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_REFRESH_APP_REFRESH_PROVIDER_H_
