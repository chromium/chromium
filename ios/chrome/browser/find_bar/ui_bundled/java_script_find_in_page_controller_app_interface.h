// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_JAVA_SCRIPT_FIND_IN_PAGE_CONTROLLER_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_JAVA_SCRIPT_FIND_IN_PAGE_CONTROLLER_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// InfobarManagerAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface JavaScriptFindInPageControllerAppInterface : NSObject

// Clears the search term in JavaScriptFindInPageController.
+ (void)clearSearchTerm;

@end

#endif  // IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_JAVA_SCRIPT_FIND_IN_PAGE_CONTROLLER_APP_INTERFACE_H_
