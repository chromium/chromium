// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View Controller for the Assistant Sheet.
@interface AssistantSheetViewController : UIViewController

// Whether the sheet is currently being animated by an external animator.
@property(nonatomic, assign) BOOL isAnimating;

// The view to anchor to. If nil, falls back to the bottom of the parent view.
@property(nonatomic, weak) UIView* anchorView;

// Whether to anchor to the bottom of the view (YES) or the top (NO).
// Defaults to NO.
@property(nonatomic, assign) BOOL anchorToBottom;

// Default initializer.
- (instancetype)initWithViewController:(UIViewController*)viewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_CONTROLLER_H_
