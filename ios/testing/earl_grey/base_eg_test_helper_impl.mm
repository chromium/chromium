// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

#import "base/debug/stack_trace.h"
#import "base/logging.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

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

  DLOG(WARNING) << "\n" << base::debug::StackTrace(/*count=*/15).ToString();

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
  [_impl handleException:exception details:reason];
}

@end
