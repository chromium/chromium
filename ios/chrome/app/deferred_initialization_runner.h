// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_
#define IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

@class DeferredInitializationQueue;

// An object to run initialization code asynchronously. Blocks are scheduled to
// be run after a delay. The block is named when scheduled so that other code
// can force a deferred block to be run synchronously if necessary.
@interface DeferredInitializationRunner : NSObject

// Initialize an instance with a specific queue.
- (instancetype)initWithQueue:(DeferredInitializationQueue*)queue
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Adds `block` to the queue of block to run. The `name` can be used to
// cancel the block (before it has been executed). If there is already
// a block scheduled with `name`, it is cancelled.
- (void)enqueueBlockNamed:(NSString*)name block:(ProceduralBlock)block;

// Looks up a previously scheduled block of `name`. If block has not been
// run yet, run it synchronously now.
- (void)runBlockNamed:(NSString*)name;

// Cancel all pending blocks.
- (void)cancelAllBlocks;

@end

#endif  // IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_
