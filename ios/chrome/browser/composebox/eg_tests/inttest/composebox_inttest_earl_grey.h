// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_INTTEST_COMPOSEBOX_INTTEST_EARL_GREY_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_INTTEST_COMPOSEBOX_INTTEST_EARL_GREY_H_

#import <Foundation/Foundation.h>

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

#define ComposeboxInttestEarlGrey                             \
  [ComposeboxInttestEarlGreyImpl invokedFromFile:@"" __FILE__ \
                                      lineNumber:__LINE__]

class GURL;

/// Methods used for the EarlGrey tests when the composebox is presented by the
/// `ComposeboxInttestCoordinator`.
@interface ComposeboxInttestEarlGreyImpl : BaseEGTestHelperImpl

/// Asserts that `searchTerms` are loaded by the composebox.
- (void)assertSearchLoaded:(NSString*)searchTerms;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_INTTEST_COMPOSEBOX_INTTEST_EARL_GREY_H_
