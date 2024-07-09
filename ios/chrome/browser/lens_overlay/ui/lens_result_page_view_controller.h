// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_consumer.h"

/// View controller of the lens result page.
@interface LensResultPageViewController
    : UIViewController <LensResultPageConsumer>

/// Container for the web view.
@property(nonatomic, strong, readonly) UIView* webViewContainer;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_VIEW_CONTROLLER_H_
