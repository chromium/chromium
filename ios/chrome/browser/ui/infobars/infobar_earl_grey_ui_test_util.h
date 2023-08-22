// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_EARL_GREY_UI_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_EARL_GREY_UI_TEST_UTIL_H_

#import <Foundation/Foundation.h>

// Test methods that perform sign in actions on Chrome UI.
@interface InfobarEarlGreyUI : NSObject

+ (void)waitUntilInfobarBannerVisibleOrTimeout:(BOOL)shouldShow;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_EARL_GREY_UI_TEST_UTIL_H_
