// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_H_

#import <UIKit/UIKit.h>

@class AppBarView;

// Delegate protocol for AppBarView, used to notify observers about
// changes to the view's window ownership.
@protocol AppBarViewDelegate <NSObject>

// Called when the view moves to a new window.
- (void)appBarViewDidMoveToWindow:(AppBarView*)view;

@end

// A container view for the App Bar that handles tracking when it is
// added to or removed from a window.
@interface AppBarView : UIView

// Delegate for handling window transition events.
@property(nonatomic, weak) id<AppBarViewDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_H_
