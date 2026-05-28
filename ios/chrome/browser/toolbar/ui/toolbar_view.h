// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

@class ToolbarView;

// Delegate protocol for ToolbarView, used to notify observers about
// changes to the view's window ownership.
@protocol ToolbarViewDelegate <NSObject>

// Called when the view moves to a new window.
- (void)toolbarViewDidMoveToWindow:(ToolbarView*)view;

@end

// A container view for the Toolbar that handles tracking when it is
// added to or removed from a window.
@interface ToolbarView : UIView

// Delegate for handling window transition events.
@property(nonatomic, weak) id<ToolbarViewDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_H_
