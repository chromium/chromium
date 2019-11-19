// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_ORIENTATION_LIMITING_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_ORIENTATION_LIMITING_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

// A navigation controller that supports only |UIInterfaceOrientationPortrait|
// orientation on iPhone and supports all orientations on iPad.
@interface OrientationLimitingNavigationController : UINavigationController
@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_ORIENTATION_LIMITING_NAVIGATION_CONTROLLER_H_
