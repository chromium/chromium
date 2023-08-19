// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/ocmock_complex_type_helper.h"

#import <ostream>

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"

@implementation OCMockComplexTypeHelper {
  // The represented object.
  OCMockObject* _object;

  // Dictionary holding blocks registered by selector.
  NSMutableDictionary<NSString*, id>* _blocks;
}

#pragma mark - Public methods.

- (instancetype)initWithRepresentedObject:(OCMockObject*)object {
  DCHECK(object);
  _object = object;
  _blocks = [[NSMutableDictionary alloc] init];
  return self;
}

- (void)onSelector:(SEL)selector callBlockExpectation:(id)block {
  NSString* key = NSStringFromSelector(selector);
  DCHECK(![_blocks objectForKey:key]) << "Only one expectation per signature";
  [_blocks setObject:block forKey:key];
}

- (id)blockForSelector:(SEL)selector {
  NSString* key = NSStringFromSelector(selector);
  id block = [_blocks objectForKey:key];
  DCHECK(block) << "Missing block expectation for selector "
                << base::SysNSStringToUTF8(key);
  return block;
}

#pragma mark - OCMockObject forwarding.

// OCMockObject -respondsToSelector responds NO for the OCMock object specific
// methods. This confuses the NSProxy architecture. In order to forward
// those properly the simplest approach is to forward them explicitely.
- (id)stub {
  return [_object stub];
}

- (id)expect {
  return [_object expect];
}

- (id)reject {
  return [_object reject];
}

- (void)verify {
  [_object verify];
}

- (void)setExpectationOrderMatters:(BOOL)flag {
  [_object setExpectationOrderMatters:flag];
}

#pragma mark - NSProxy implementation.

- (void)forwardInvocation:(NSInvocation*)invocation {
  SEL selector = [invocation selector];
  if ([_object respondsToSelector:selector])
    [invocation invokeWithTarget:_object];
}

- (NSMethodSignature*)methodSignatureForSelector:(SEL)selector {
  return [_object methodSignatureForSelector:selector];
}

- (void)doesNotRecognizeSelector:(SEL)selector {
  [(id)_object doesNotRecognizeSelector:selector];
}

- (BOOL)respondsToSelector:(SEL)selector {
  DCHECK(![_blocks objectForKey:NSStringFromSelector(selector)]);
  if (selector == @selector(initWithRepresentedObject:))
    return YES;

  return [_object respondsToSelector:selector];
}

@end
