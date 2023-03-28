// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CHROME_ACTIVITY_OVERLAY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CHROME_ACTIVITY_OVERLAY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View controller that displays a UIActivityIndicatorView and informative
// `messageText` over a translucent background.
@interface ChromeActivityOverlayViewController : UIViewController

// Text that will be shown above the UIActivityIndicatorView.
@property(nonatomic, copy) NSString* messageText;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CHROME_ACTIVITY_OVERLAY_VIEW_CONTROLLER_H_
