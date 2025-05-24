// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_DEFERRED_INITIALIZATION_QUEUE_H_
#define IOS_CHROME_APP_DEFERRED_INITIALIZATION_QUEUE_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "base/time/time.h"

// Reference to a scheduled block (can be used to cancel it).
@class DeferredInitializationBlock;

// An object to manage a queue of blocks to run asynchronously.
@interface DeferredInitializationQueue : NSObject

// Returns a singleton instance.
+ (instancetype)sharedInstance;

// Initializes an instance with specified delays.
- (instancetype)initWithDelayBetweenBlocks:(base::TimeDelta)betweenBlocks
                     delayBeforeFirstBlock:(base::TimeDelta)beforeFirstBlock
    NS_DESIGNATED_INITIALIZER;

// Initializes an instance with default delays.
- (instancetype)init;

// Schedules `block` to be executed asynchronously.
- (DeferredInitializationBlock*)enqueueBlock:(ProceduralBlock)block;

// Executes `block` immediately.
- (void)runBlock:(DeferredInitializationBlock*)block;

// Cancels `block` if it has not been run yet.
- (void)cancelBlock:(DeferredInitializationBlock*)block;

// Cancels the blocks from `deferredBlocks` if they has not been run yet.
- (void)cancelBlocks:(NSArray<DeferredInitializationBlock*>*)deferredBlocks;

// Returns the size of the queue (i.e. the number of blocks scheduled pending).
@property(nonatomic, readonly) NSUInteger length;

@end

#endif  // IOS_CHROME_APP_DEFERRED_INITIALIZATION_QUEUE_H_
