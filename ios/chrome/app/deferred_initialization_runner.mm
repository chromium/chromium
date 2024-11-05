// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/deferred_initialization_runner.h"

#import <stdint.h>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/sequence_checker.h"
#import "base/timer/timer.h"

NSString* const kPrefObserverInit = @"PrefObserverInit";

// An object encapsulating the deferred execution of a block of initialization
// code.
@interface DeferredInitializationBlock : NSObject

// Name of the block.
@property(nonatomic, readonly) NSString* name;

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)initWithName:(NSString*)name
                       block:(ProceduralBlock)block NS_DESIGNATED_INITIALIZER;

// Executes the deferred block now.
- (void)run;

@end

@implementation DeferredInitializationBlock {
  // A block of code to execute.
  ProceduralBlock _runBlock;
}

- (instancetype)initWithName:(NSString*)name block:(ProceduralBlock)block {
  DCHECK(block);
  self = [super init];
  if (self) {
    _name = [name copy];
    _runBlock = block;
  }
  return self;
}

- (void)run {
  ProceduralBlock deferredBlock = nil;
  std::swap(deferredBlock, _runBlock);
  if (deferredBlock) {
    deferredBlock();
  }
}

@end

@implementation DeferredInitializationRunner {
  // The list of pending blocks.
  NSMutableArray<DeferredInitializationBlock*>* _runBlocks;

  // The timer used to schedule the execution of the next block.
  base::OneShotTimer _timer;

  // Time interval between two blocks.
  base::TimeDelta _delayBetweenBlocks;

  // Time interval before running the first block.
  base::TimeDelta _delayBeforeFirstBlock;

  SEQUENCE_CHECKER(_sequenceChecker);
}

+ (DeferredInitializationRunner*)sharedInstance {
  static dispatch_once_t once = 0;
  static DeferredInitializationRunner* instance = nil;
  dispatch_once(&once, ^{
    instance = [[DeferredInitializationRunner alloc] init];
  });
  return instance;
}

- (instancetype)initWithDelayBetweenBlocks:(base::TimeDelta)betweenBlocks
                     delayBeforeFirstBlock:(base::TimeDelta)beforeFirstBlock {
  if ((self = [super init])) {
    _runBlocks = [NSMutableArray array];
    _delayBetweenBlocks = betweenBlocks;
    _delayBeforeFirstBlock = beforeFirstBlock;
  }
  return self;
}

- (instancetype)init {
  return [self initWithDelayBetweenBlocks:base::Milliseconds(200)
                    delayBeforeFirstBlock:base::Seconds(3)];
}

- (void)enqueueBlockNamed:(NSString*)name block:(ProceduralBlock)block {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(name);
  DCHECK(block);

  DeferredInitializationBlock* deferredBlock =
      [[DeferredInitializationBlock alloc] initWithName:name block:block];
  [_runBlocks addObject:deferredBlock];

  if (!_timer.IsRunning()) {
    __weak DeferredInitializationRunner* weakSelf = self;
    _timer.Start(FROM_HERE, _delayBeforeFirstBlock, base::BindOnce(^{
                   [weakSelf runNextBlock];
                 }));
  }
}

- (void)runNextBlock {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DeferredInitializationBlock* block = nil;
  if (_runBlocks.count > 0) {
    block = [_runBlocks objectAtIndex:0];
    [_runBlocks removeObjectAtIndex:0];
  }

  if (_runBlocks.count > 0) {
    __weak DeferredInitializationRunner* weakSelf = self;
    _timer.Start(FROM_HERE, _delayBetweenBlocks, base::BindOnce(^{
                   [weakSelf runNextBlock];
                 }));
  }

  if (block) {
    [block run];
  }
}

- (void)runBlockIfNecessary:(NSString*)name {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(name);
  DeferredInitializationBlock* block = [self popBlockNamed:name];
  if (block) {
    [block run];
  }
}

- (void)cancelBlockNamed:(NSString*)name {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(name);
  [self popBlockNamed:name];
}

- (NSUInteger)numberOfBlocksRemaining {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return [_runBlocks count];
}

- (DeferredInitializationBlock*)popBlockNamed:(NSString*)name {
  NSUInteger count = _runBlocks.count;
  for (NSUInteger index = 0; index < count; ++index) {
    DeferredInitializationBlock* block = [_runBlocks objectAtIndex:index];
    if ([block.name isEqualToString:name]) {
      [_runBlocks removeObjectAtIndex:0];
      return block;
    }
  }
  return nil;
}

@end
