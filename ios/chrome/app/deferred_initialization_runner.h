// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_
#define IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "base/time/time.h"

// Constants for deferred initialization of preferences observer.
extern NSString* const kPrefObserverInit;

// An object to run initialization code asynchronously. Blocks are scheduled to
// be run after a delay. The block is named when added to the singleton so that
// other code can force a deferred block to be run synchronously if necessary.
@interface DeferredInitializationRunner : NSObject

// Returns a singleton instance.
+ (DeferredInitializationRunner*)sharedInstance;

// Initialize an instance with specified delays.
- (instancetype)initWithDelayBetweenBlocks:(base::TimeDelta)betweenBlocks
                     delayBeforeFirstBlock:(base::TimeDelta)beforeFirstBlock
    NS_DESIGNATED_INITIALIZER;

// Initialize an instance with default delays.
- (instancetype)init;

// Stores `block` under `name` to a queue of blocks to run. All blocks are run
// sequentially with a small delay before the first block and between each
// successive block.
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

#endif  // IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_
