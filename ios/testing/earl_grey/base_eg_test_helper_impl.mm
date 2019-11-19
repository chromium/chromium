// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

#include "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BaseEGTestHelperImpl {
  // Used to raise EarlGrey exception with specific file name and line number.
  EarlGreyImpl* _impl;
}

+ (instancetype)invokedFromFile:(NSString*)fileName lineNumber:(int)lineNumber {
  EarlGreyImpl* impl = [EarlGreyImpl invokedFromFile:fileName
                                          lineNumber:lineNumber];
  return [[[self class] alloc] initWithImpl:impl];
}

- (instancetype)initWithImpl:(EarlGreyImpl*)impl {
  self = [super init];
  if (self) {
    _impl = impl;
  }
  return self;
}

- (void)failWithError:(NSError*)error expression:(NSString*)expression {
  if (!error)
    return;

  NSString* name = [NSString stringWithFormat:@"%@ helper error", [self class]];
  NSString* reason =
      [NSString stringWithFormat:@"%@ expression returned error: '%@'",
                                 expression, error.localizedDescription];
  [self failWithExceptionName:name reason:reason];
}

#pragma mark - Private

- (void)fail:(BOOL)fail
     expression:(NSString*)expression
    description:(NSString*)description {
  if (!fail)
    return;

  NSString* reason =
      [NSString stringWithFormat:@"%@ is false: %@", expression, description];
  [self failWithExceptionName:@"expression error" reason:reason];
}

- (EarlGreyImpl*)earlGrey {
  return _impl;
}

#pragma mark - Private

- (void)failWithExceptionName:(NSString*)name reason:(NSString*)reason {
  GREYFrameworkException* exception =
      [GREYFrameworkException exceptionWithName:name reason:reason];
  [_impl handleException:exception details:@""];
}

@end
