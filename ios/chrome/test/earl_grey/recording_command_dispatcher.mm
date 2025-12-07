// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/recording_command_dispatcher.h"

#import <unordered_map>

// An object that records the selectors that are invoked on it in an array.
@interface DispatchRecorder : NSProxy
@property(nonatomic, strong) NSMutableArray<NSString*>* dispatches;
@end

@implementation DispatchRecorder {
  std::unordered_map<SEL, ProceduralBlock> _callbacks;
}

- (instancetype)init {
  _dispatches = [[NSMutableArray alloc] init];
  return self;
}

- (void)setAction:(ProceduralBlock)block forSelector:(SEL)selector {
  _callbacks[selector] = block;
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  [self.dispatches addObject:NSStringFromSelector(invocation.selector)];

  ProceduralBlock block = _callbacks[invocation.selector];
  if (block) {
    block();
  }
}

- (NSMethodSignature*)methodSignatureForSelector:(SEL)selector {
  // Return some method signature to silence errors.
  // Here it's (void)(self, _cmd).
  return [NSMethodSignature signatureWithObjCTypes:"v@:"];
}

@end

@implementation RecordingCommandDispatcher {
  DispatchRecorder* _recorder;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _recorder = [[DispatchRecorder alloc] init];
  }
  return self;
}

- (BOOL)dispatchingForProtocol:(Protocol*)protocol {
  if (![super dispatchingForProtocol:protocol]) {
    [self startDispatchingToTarget:_recorder forProtocol:protocol];
  }
  return YES;
}

- (NSArray<NSString*>*)dispatches {
  return _recorder.dispatches;
}

- (void)setAction:(ProceduralBlock)block forSelector:(SEL)selector {
  [_recorder setAction:block forSelector:selector];
}

@end
