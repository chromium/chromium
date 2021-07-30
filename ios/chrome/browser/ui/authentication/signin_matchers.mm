// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_matchers.h"

#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

id<GREYMatcher> IdentityCellMatcherForEmail(NSString* email) {
  return [SigninEarlGreyAppInterface identityCellMatcherForEmail:email];
}

}  // namespace chrome_test_util
