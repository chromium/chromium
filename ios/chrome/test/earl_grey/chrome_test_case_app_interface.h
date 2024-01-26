// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_TEST_CASE_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_TEST_CASE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@interface ChromeTestCaseAppInterface : NSObject

// Sets up mock authentication and the mock account reconcilor.
+ (void)setUpMockAuthentication;

// Tears down mock authentication and the mock account reconcilor.
+ (void)tearDownMockAuthentication;

// Resets mock authentication and signin promo settings.
+ (void)resetAuthentication;

// Removes all infobars and clears any presented state.
// See +[ChromeTestCaseAppInterface isCompletionInvokedWithUUID:] to know when
// all views are dismissed.
+ (void)removeInfoBarsAndPresentedStateWithCompletionUUID:
    (NSUUID*)completionUUID;

// Blocks signin IPH from triggering in egtests.
+ (void)blockSigninIPH;

// Returns YES if the completion related to `completionUUID` has been invoked.
// Once this method returns YES, `completionUUID` is dropped, and a second call
// will return NO.
+ (BOOL)isCompletionInvokedWithUUID:(NSUUID*)completionUUID;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_TEST_CASE_APP_INTERFACE_H_
