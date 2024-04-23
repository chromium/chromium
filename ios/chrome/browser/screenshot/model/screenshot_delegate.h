// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREENSHOT_MODEL_SCREENSHOT_DELEGATE_H_
#define IOS_CHROME_BROWSER_SCREENSHOT_MODEL_SCREENSHOT_DELEGATE_H_

#import <UIKit/UIKit.h>

// TODO(crbug.com/40670317): Refactor this class to not use
// BrowserProviderInterface when possible.
@protocol BrowserProviderInterface;

// ScreenshotDelegate provides methods for UIScreenshotServiceDelegate to create
// PDF content of the captured window scene.
@interface ScreenshotDelegate : NSObject <UIScreenshotServiceDelegate>

// Init the ScreenshotDelegate and set the `browserProviderInterface` to
// generate PDF screenshots from.
- (instancetype)initWithBrowserProviderInterface:
    (id<BrowserProviderInterface>)browserProviderInterface
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SCREENSHOT_MODEL_SCREENSHOT_DELEGATE_H_
