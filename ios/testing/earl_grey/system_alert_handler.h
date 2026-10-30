// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_SYSTEM_ALERT_HANDLER_H_
#define IOS_TESTING_EARL_GREY_SYSTEM_ALERT_HANDLER_H_

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

#define SystemAlertHandler \
  [SystemAlertHandlerImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

@interface SystemAlertHandlerImpl : BaseEGTestHelperImpl

// Handles currently visible system alerts, closing them to unblock the UI.
// Does not wait for in-flight alerts to appear. Asks the app under test, so it
// is not guaranteed to detect alerts raised by a previous app process.
- (void)handleSystemAlertIfVisible;

// Same as `-handleSystemAlertIfVisible`, but also detects alerts left behind by
// a previous app process (e.g. a permission prompt from a test that failed
// before dismissing it). More expensive; intended to be called once after the
// app is (re)launched.
- (void)handleLeftoverSystemAlertIfVisible;

@end

#endif  // IOS_TESTING_EARL_GREY_SYSTEM_ALERT_HANDLER_H_
