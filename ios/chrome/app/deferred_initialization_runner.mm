// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/deferred_initialization_runner.h"

#import <stdint.h>

#import "base/check.h"

NSString* const kPrefObserverInit = @"PrefObserverInit";

// An object encapsulating the deferred execution of a block of initialization
// code.
@interface DeferredInitializationBlock : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)initWithName:(NSString*)name
                       block:(ProceduralBlock)block NS_DESIGNATED_INITIALIZER;

// Executes the deferred block now.
- (void)run;

// Cancels the block's execution.
- (void)cancel;

@end

@implementation DeferredInitializationBlock {
  // A string to reference the initialization block.
  NSString* _name;
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
  DCHECK([NSThread isMainThread]);
  ProceduralBlock deferredBlock = _runBlock;
  if (!deferredBlock)
    return;
  deferredBlock();
  [[DeferredInitializationRunner sharedInstance] cancelBlockNamed:_name];
}

- (void)cancel {
  _runBlock = nil;
}

@end

@interface DeferredInitializationRunner () {
  NSMutableArray* _blocksNameQueue;
  NSMutableDictionary* _runBlocks;
  BOOL _isBlockScheduled;
}

// Schedule the next block to be run after `delay` it will automatically
// schedule the next block after `delayBetweenBlocks`.
- (void)scheduleNextBlockWithDelay:(NSTimeInterval)delay;

// Time interval between two blocks. Default value is 200ms.
@property(nonatomic) NSTimeInterval delayBetweenBlocks;

// Time interval before running the first block. Default value is 3s.
@property(nonatomic) NSTimeInterval delayBeforeFirstBlock;

@end

@implementation DeferredInitializationRunner

@synthesize delayBetweenBlocks = _delayBetweenBlocks;
@synthesize delayBeforeFirstBlock = _delayBeforeFirstBlock;

+ (DeferredInitializationRunner*)sharedInstance {
  static dispatch_once_t once = 0;
  static DeferredInitializationRunner* instance = nil;
  dispatch_once(&once, ^{
    instance = [[DeferredInitializationRunner alloc] init];
  });
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _blocksNameQueue = [NSMutableArray array];
    _runBlocks = [NSMutableDictionary dictionary];
    _isBlockScheduled = NO;
    _delayBetweenBlocks = 0.2;
    _delayBeforeFirstBlock = 3.0;
  }
  return self;
}

- (void)enqueueBlockNamed:(NSString*)name block:(ProceduralBlock)block {
  DCHECK(name);
  DCHECK([NSThread isMainThread]);
  [self cancelBlockNamed:name];
  [_blocksNameQueue addObject:name];

  DeferredInitializationBlock* deferredBlock =
      [[DeferredInitializationBlock alloc] initWithName:name block:block];
  [_runBlocks setObject:deferredBlock forKey:name];

  if (!_isBlockScheduled) {
    [self scheduleNextBlockWithDelay:self.delayBeforeFirstBlock];
  }
}

- (void)scheduleNextBlockWithDelay:(NSTimeInterval)delay {
  DCHECK([NSThread isMainThread]);
  _isBlockScheduled = NO;
  NSString* nextBlockName = [_blocksNameQueue firstObject];
  if (!nextBlockName)
    return;

  DeferredInitializationBlock* nextBlock =
      [_runBlocks objectForKey:nextBlockName];
  DCHECK(nextBlock);

  __weak DeferredInitializationRunner* weakSelf = self;

  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [nextBlock run];
        [weakSelf scheduleNextBlockWithDelay:[weakSelf delayBetweenBlocks]];
      });

  _isBlockScheduled = YES;
  [_blocksNameQueue removeObjectAtIndex:0];
}

- (void)runBlockIfNecessary:(NSString*)name {
  DCHECK([NSThread isMainThread]);
  [[_runBlocks objectForKey:name] run];
}

- (void)cancelBlockNamed:(NSString*)name {
  DCHECK([NSThread isMainThread]);
  DCHECK(name);
  [_blocksNameQueue removeObject:name];
  [[_runBlocks objectForKey:name] cancel];
  [_runBlocks removeObjectForKey:name];
}

- (NSUInteger)numberOfBlocksRemaining {
  return [_runBlocks count];
}

@end
