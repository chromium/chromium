// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

@protocol AppBarContainerViewDelegate;

// Container view for the App Bar. It is in charge of positioning and layout
// based on the window size.
@interface AppBarContainerView : UIView

// Delegate for rotation and window events.
@property(nonatomic, weak) id<AppBarContainerViewDelegate> delegate;

// Sets the App Bar view to be contained.
- (void)setAppBar:(UIView*)appBar;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_H_
