// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"

#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WebShellTestCase

#if defined(CHROME_EARL_GREY_1)
// Overrides |testInvocations| to skip all tests if a system alert view is
// shown, since this isn't a case a user would encounter (i.e. they would
// dismiss the alert first).
+ (NSArray*)testInvocations {
  // TODO(crbug.com/654085): Simply skipping all tests isn't the best way to
  // handle this, it would be better to have something that is more obvious
  // on the bots that this is wrong, without making it look like test flake.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:grey_systemAlertViewShown()]
      assertWithMatcher:grey_nil()
                  error:&error];
  if (error != nil) {
    NSLog(@"System alert view is present, so skipping all tests!");
    return @[];
  }
  return [super testInvocations];
}
#endif

@end
