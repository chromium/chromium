// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_CHROME_ICON_H_
#define IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_CHROME_ICON_H_

#import <UIKit/UIKit.h>

@interface ChromeIcon : NSObject

// Commonly used icons. Icons that flip in RTL already have their
// flipsForRightToLeftLayoutDirection
// property set accordingly.
+ (UIImage*)backIcon;
+ (UIImage*)closeIcon;
+ (UIImage*)infoIcon;
+ (UIImage*)searchIcon;
+ (UIImage*)chevronIcon;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_CHROME_ICON_H_
