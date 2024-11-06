// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/deferred_initialization_queue.h"

#import "base/check.h"
#import "base/sequence_checker.h"
#import "base/timer/timer.h"

@interface DeferredInitializationBlock : NSObject

- (instancetype)initWithBlock:(ProceduralBlock)block NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)run;

@end

@implementation DeferredInitializationBlock {
  ProceduralBlock _block;
}

- (instancetype)initWithBlock:(ProceduralBlock)block {
  if ((self = [super init])) {
    _block = block;
  }
  return self;
}

- (void)run {
  std::exchange(_block, nil)();
}

@end

@implementation DeferredInitializationQueue {
  // The queue of pending blocks to execute.
  NSMutableArray<DeferredInitializationBlock*>* _queue;

  // Time interval between two blocks.
  base::TimeDelta _delayBetweenBlocks;

  // Time interval before running the first block.
  base::TimeDelta _delayBeforeFirstBlock;

  // Timer used to schedule execution of the blocks.
  base::OneShotTimer _timer;

  // Sequence-checker used to enforce sequence-affinity.
  SEQUENCE_CHECKER(_sequenceChecker);
}

+ (instancetype)sharedInstance {
  static DeferredInitializationQueue* gInstance =
      [[DeferredInitializationQueue alloc] init];
  return gInstance;
}

- (instancetype)initWithDelayBetweenBlocks:(base::TimeDelta)betweenBlocks
                     delayBeforeFirstBlock:(base::TimeDelta)beforeFirstBlock {
  if ((self = [super init])) {
    _queue = [NSMutableArray array];
    _delayBetweenBlocks = betweenBlocks;
    _delayBeforeFirstBlock = beforeFirstBlock;
  }
  return self;
}

- (instancetype)init {
  return [self initWithDelayBetweenBlocks:base::Milliseconds(200)
                    delayBeforeFirstBlock:base::Seconds(3)];
}

- (DeferredInitializationBlock*)enqueueBlock:(ProceduralBlock)block {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(block);

  DeferredInitializationBlock* deferredBlock =
      [[DeferredInitializationBlock alloc] initWithBlock:block];
  [_queue addObject:deferredBlock];

  if (!_timer.IsRunning()) {
    [self scheduleExecution:_delayBeforeFirstBlock];
  }

  return deferredBlock;
}

- (void)runBlock:(DeferredInitializationBlock*)block {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSUInteger index = [_queue indexOfObject:block];
  if (index != NSNotFound) {
    [_queue removeObjectAtIndex:index];
    [block run];
  }
}

- (void)cancelBlock:(DeferredInitializationBlock*)block {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_queue removeObject:block];
}

- (void)cancelBlocks:(NSArray<DeferredInitializationBlock*>*)deferredBlocks {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_queue removeObjectsInArray:deferredBlocks];
}

#pragma mark Properties

- (NSUInteger)length {
  return _queue.count;
}

#pragma mark Private methods

// Schedules -runNextBlock to be executed after `delay`.
- (void)scheduleExecution:(base::TimeDelta)delay {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(!_timer.IsRunning());
  DCHECK_GT(_queue.count, 0u);

  __weak DeferredInitializationQueue* weakSelf = self;
  _timer.Start(FROM_HERE, delay, base::BindOnce(^{
                 [weakSelf runNextBlock];
               }));
}

// Executes the next pending block and if there are any block remaining,
// schedule -runNextBlock to be invoked again.
- (void)runNextBlock {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(!_timer.IsRunning());

  DeferredInitializationBlock* handle = nil;
  if (_queue.count > 0) {
    handle = [_queue objectAtIndex:0];
    [_queue removeObjectAtIndex:0];
    if (_queue.count > 0) {
      // Ensure the method is called again if there are still block in the
      // queue after poping the first item.
      [self scheduleExecution:_delayBetweenBlocks];
    }
  }

  [handle run];
}

@end
