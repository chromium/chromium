// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_EARL_GREY_H_
#define IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_EARL_GREY_H_

#import <Foundation/Foundation.h>

#include "base/time/time.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"
#include "url/gurl.h"

@protocol GREYMatcher;
// Public macro to invoke helper methods in test methods (Test Process). Usage
// example:
//
// @interface PageLoadTestCase : XCTestCase
// @end
// @implementation PageLoadTestCase
// - (void)testPageload {
//   [ShellEarlGrey loadURL:GURL("https://chromium.org")];
// }
//
// In this example ShellEarlGreyImpl must implement -loadURL:.
//
#define ShellEarlGrey \
  [ShellEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// Used for logging the failure. Compiled in Test Process for EG2 and EG1. Can
// be extended with category methods to provide additional test helpers.
// Category method names must be unique.
@interface ShellEarlGreyImpl : BaseEGTestHelperImpl

// Loads `URL` in the current WebState with transition of type
// ui::PAGE_TRANSITION_TYPED and waits for the loading to complete. Raises
// EarlGrey exception if load does not complete within a timeout.
- (void)loadURL:(const GURL&)URL;

// Waits for the current web view to contain `text`. Raises EarlGrey exception
// if the content does not show up within a timeout.
- (void)waitForWebStateContainingText:(NSString*)text;

// Waits for the matcher to not return any elements.
- (void)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher;

// Waits for the matcher to not return any elements. If the condition is not met
// within the given `timeout` a GREYAssert is induced.
- (void)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher
                                       timeout:(base::TimeDelta)timeout;

@end

#endif  // IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_EARL_GREY_H_
