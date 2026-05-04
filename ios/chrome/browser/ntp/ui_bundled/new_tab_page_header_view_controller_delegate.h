// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class NewTabPageHeaderViewController;

// Delegate for the NewTabPageHeaderViewController.
@protocol NewTabPageHeaderViewControllerDelegate

// Whether the scrollview is scrolled to the omnibox.
@property(nonatomic, assign, readonly) BOOL scrolledToMinimumHeight;

// Notifies the delegate when the omnibox position is updated in the
// `viewController`.
- (void)didChangeOmniboxPosition:
    (NewTabPageHeaderViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_CONTROLLER_DELEGATE_H_
