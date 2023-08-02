// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"

#import "testing/platform_test.h"

namespace {
void ExpectErrorInMessage(NSError* error,
                          NSString* message,
                          BOOL hasDescription) {
  if (hasDescription) {
    EXPECT_TRUE([message containsString:error.localizedDescription]);
  } else if (error.userInfo[NSLocalizedDescriptionKey]) {
    EXPECT_FALSE([message containsString:error.localizedDescription]);
  }
  EXPECT_TRUE([message containsString:error.domain]);
  EXPECT_TRUE([message containsString:[@(error.code) description]]);
}
}

using AuthenticationUIUtil = PlatformTest;

// Tests the error message with one error with a localized description.
TEST_F(AuthenticationUIUtil, DialogMessageFromErrorWithLocalizedDescription) {
  NSDictionary* userInfo =
      @{NSLocalizedDescriptionKey : @"MyLocalizedDescription"};
  NSError* error =
      [NSError errorWithDomain:@"MyErrorDomain" code:-1234 userInfo:userInfo];
  NSString* message = DialogMessageFromError(error);
  ExpectErrorInMessage(error, message, YES);
}

// Tests the error message with one error without a localized description.
TEST_F(AuthenticationUIUtil,
       DialogMessageFromErrorWithoutLocalizedDescription) {
  NSError* error =
      [NSError errorWithDomain:@"MyErrorDomain" code:-1234 userInfo:nil];
  NSString* message = DialogMessageFromError(error);
  ExpectErrorInMessage(error, message, NO);
}

// Tests the error message with an error with 2 underlying errors.
TEST_F(AuthenticationUIUtil, DialogMessageFromErrorWithUnderlyingErrors) {
  // Error 1
  NSDictionary* userInfo1 =
      @{NSLocalizedDescriptionKey : @"MyLocalizedDescription1"};
  NSError* error1 =
      [NSError errorWithDomain:@"MyErrorDomain1" code:-1234 userInfo:userInfo1];

  // Error 2
  NSDictionary* userInfo2 = @{
    NSLocalizedDescriptionKey : @"MyLocalizedDescription2",
    NSUnderlyingErrorKey : error1
  };
  NSError* error2 =
      [NSError errorWithDomain:@"MyErrorDomain2" code:-567 userInfo:userInfo2];

  // Error 3
  NSDictionary* userInfo3 = @{
    NSLocalizedDescriptionKey : @"MyLocalizedDescription3",
    NSUnderlyingErrorKey : error2
  };
  NSError* error3 =
      [NSError errorWithDomain:@"MyErrorDomain3" code:-890 userInfo:userInfo3];

  NSString* message = DialogMessageFromError(error3);
  ExpectErrorInMessage(error3, message, YES);
  ExpectErrorInMessage(error2, message, NO);
  ExpectErrorInMessage(error1, message, NO);
}
