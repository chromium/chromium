// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_MANAGER_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_MANAGER_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// InfobarManagerAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface InfobarManagerAppInterface : NSObject

// Verifies that there are |totalInfobars| in the InfobarManager of the current
// active WebState.
+ (BOOL)verifyInfobarCount:(NSInteger)totalInfobars;

// Adds a TestInfoBar with |message| to the current active WebState.
+ (BOOL)addTestInfoBarToCurrentTabWithMessage:(NSString*)message;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_MANAGER_APP_INTERFACE_H_
