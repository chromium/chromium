// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_UIKIT_UI_UTIL_BRIDGE_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_UIKIT_UI_UTIL_BRIDGE_H_

#import <UIKit/UIKit.h>

// An Objective-C wrapper around C++ UiKit utils.
@interface UiKitUtils : NSObject

+ (UIImage*)greyImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_UIKIT_UI_UTIL_BRIDGE_H_
