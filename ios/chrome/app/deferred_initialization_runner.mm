// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/deferred_initialization_runner.h"

#import <stdint.h>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/sequence_checker.h"
#import "base/timer/timer.h"
#import "ios/chrome/app/deferred_initialization_queue.h"

@implementation DeferredInitializationRunner {
  // Queue used to schedule the blocks.
  DeferredInitializationQueue* _queue;

  // Dictionary of named deferred blocks.
  NSMutableDictionary<NSString*, DeferredInitializationBlock*>* _blocks;

  // Sequence-checker used to enforce sequence-affinity.
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithQueue:(DeferredInitializationQueue*)queue {
  if ((self = [super init])) {
    DCHECK(queue);
    _queue = queue;
    _blocks = [NSMutableDictionary dictionary];
  }
  return self;
}

- (void)enqueueBlockNamed:(NSString*)name block:(ProceduralBlock)block {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK_GT(name.length, 0u);
  DCHECK(block);

  DeferredInitializationBlock* deferredBlock = [_blocks objectForKey:name];
  if (deferredBlock) {
    [_blocks removeObjectForKey:name];
    [_queue cancelBlock:deferredBlock];
    deferredBlock = nil;
  }

  __weak DeferredInitializationRunner* weakSelf = self;
  deferredBlock = [_queue enqueueBlock:^{
    [weakSelf removeBlockNamed:name completion:block];
  }];

  [_blocks setObject:deferredBlock forKey:name];
}

- (void)runBlockNamed:(NSString*)name {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK_GT(name.length, 0u);

  DeferredInitializationBlock* deferredBlock = [_blocks objectForKey:name];
  if (deferredBlock) {
    [_queue runBlock:deferredBlock];
  }
}

- (void)cancelAllBlocks {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_queue cancelBlocks:[_blocks allValues]];
  _blocks = [NSMutableDictionary dictionary];
}

#pragma mark Private methods

- (void)removeBlockNamed:(NSString*)name
              completion:(ProceduralBlock)completion {
  [_blocks removeObjectForKey:name];
  completion();
}

@end
