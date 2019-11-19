// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_TEST_SHOWCASE_TEST_CASE_APP_INTERFACE_H_
#define IOS_SHOWCASE_TEST_SHOWCASE_TEST_CASE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App interface for the Showcase base TestCase class.
@interface ShowcaseTestCaseAppInterface : NSObject

// Sets up the UI for showcase.
+ (void)setupUI;

@end

#endif  // IOS_SHOWCASE_TEST_SHOWCASE_TEST_CASE_APP_INTERFACE_H_
