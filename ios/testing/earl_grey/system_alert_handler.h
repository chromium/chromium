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
// Does not wait for in-flight alerts to appear.
- (void)handleSystemAlertIfVisible;

@end

#endif  // IOS_TESTING_EARL_GREY_SYSTEM_ALERT_HANDLER_H_
