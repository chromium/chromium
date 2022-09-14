// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_
#define IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"

// Constants for deferred initialization of preferences observer.
extern NSString* const kPrefObserverInit;

// A singleton object to run initialization code asynchronously. Blocks are
// scheduled to be run after a delay. The block is named when added to the
// singleton so that other code can force a deferred block to be run
// synchronously if necessary.
@interface DeferredInitializationRunner : NSObject

// Returns singleton instance.
+ (DeferredInitializationRunner*)sharedInstance;

// Stores `block` under `name` to a queue of blocks to run. All blocks are run
// sequentially with a small delay before the first block and between each
// successive block. If a block is already registered under `name`, it is
// replaced with `block` unless it has already been run.
- (void)enqueueBlockNamed:(NSString*)name block:(ProceduralBlock)block;

// Looks up a previously scheduled block of `name`. If block has not been
// run yet, run it synchronously now.
- (void)runBlockIfNecessary:(NSString*)name;

// Cancels a previously scheduled block of `name`. This is a no-op if the
// block has already been executed.
- (void)cancelBlockNamed:(NSString*)name;

// Number of blocks that have been registered but not executed yet.
// Exposed for testing.
@property(nonatomic, readonly) NSUInteger numberOfBlocksRemaining;

@end

@interface DeferredInitializationRunner (ExposedForTesting)

// Time interval between two blocks. Default value is 200ms.
@property(nonatomic, assign) NSTimeInterval delayBetweenBlocks;

// Time interval before running the first block. To override default value of
// 3s, set this property before the first call to `-enqueueBlockNamed:block:`.
@property(nonatomic, assign) NSTimeInterval delayBeforeFirstBlock;

@end

#endif  // IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_
