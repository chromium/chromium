// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_BASE_EARL_GREY_TEST_CASE_APP_INTERFACE_H_
#define IOS_TESTING_EARL_GREY_BASE_EARL_GREY_TEST_CASE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// BaseEarlGreyTestCaseAppInterface contains helpers for BaseEarlGreyTestCase
// that are compiled into the app binary and can be called from either app or
// test code.
@interface BaseEarlGreyTestCaseAppInterface : NSObject

// Logs |message| from the app process (as opposed to the test process).
+ (void)logMessage:(NSString*)message;

@end

#endif  // IOS_TESTING_EARL_GREY_BASE_EARL_GREY_TEST_CASE_APP_INTERFACE_H_
