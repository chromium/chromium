// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MATERIAL_COMPONENTS_CHROME_APP_BAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_MATERIAL_COMPONENTS_CHROME_APP_BAR_VIEW_CONTROLLER_H_

#import <MaterialComponents/MDCAppBarViewController.h>

// Used as substitute to MDCAppBarViewController to prevent default behavior of
// managing accessibility dismiss gesture itself. Usually the view controller or
// its navigation controller has logic that handles it.
@interface ChromeAppBarViewController : MDCAppBarViewController
@end

#endif  // IOS_CHROME_BROWSER_UI_MATERIAL_COMPONENTS_CHROME_APP_BAR_VIEW_CONTROLLER_H_
