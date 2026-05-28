// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_DELEGATE_H_

#import <UIKit/UIKit.h>

@class NewTabPageHeaderView;

// Delegate for the NewTabPageHeaderView.
@protocol NewTabPageHeaderViewDelegate

// Whether the scrollview is scrolled to the omnibox.
@property(nonatomic, assign, readonly) BOOL scrolledToMinimumHeight;

// Whether the fake omnibox should pin to the top.
- (BOOL)shouldPinFakeOmnibox;

// Notifies the delegate when the omnibox position is updated in the
// `headerView`.
- (void)didChangeOmniboxPosition:(NewTabPageHeaderView*)headerView;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_DELEGATE_H_
