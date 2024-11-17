// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_LAST_VISITED_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_LAST_VISITED_VIEW_CONTROLLER_DELEGATE_H_

@class PageInfoLastVisitedViewController;

// Delegate for PageInfoLastVisitedViewController.
@protocol PageInfoLastVisitedViewControllerDelegate

// Notifies the coordinator that the full history should be displayed.
- (void)displayFullHistory;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_LAST_VISITED_VIEW_CONTROLLER_DELEGATE_H_
