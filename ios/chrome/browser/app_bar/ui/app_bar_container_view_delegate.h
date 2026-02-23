// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AppBarContainerView;

// Delegate for `AppBarContainerView` events.
@protocol AppBarContainerViewDelegate

// Called when the container view moves to a window.
- (void)appBarContainerDidMoveToWindow:(AppBarContainerView*)appBarContainer;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_DELEGATE_H_
