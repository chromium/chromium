// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_VIEW_CONTROLLER_H_

#import <ScreenTime/ScreenTime.h>

#import "ios/chrome/browser/screen_time/ui_bundled/screen_time_consumer.h"

// The view controller which is used to integrate ScreenTime support. This
// object is used to report web usage and block restricted webpages. To properly
// report web usage, add this view controller's view above the web view,
// entirely covering the frame of the web view. This view will automatically
// block when the underlying web view's URL becomes restricted.
@interface ScreenTimeViewController : STWebpageController <ScreenTimeConsumer>

+ (instancetype)sharedInstance;

+ (instancetype)sharedOTRInstance;
@end

#endif  // IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_VIEW_CONTROLLER_H_
