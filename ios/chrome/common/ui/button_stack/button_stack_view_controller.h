// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_VIEW_CONTROLLER_H_
#define IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/button_stack/button_stack_consumer.h"
#import "ios/chrome/common/ui/util/chrome_button.h"

@class ButtonStackConfiguration;
@protocol ButtonStackActionDelegate;

// A view controller that displays a stack of action buttons at the bottom of
// the screen. The content above the buttons is provided by subclasses in the
// `contentView`, which is embedded in a scroll view to support content of any
// length.
//
// The layout is structured as follows:
//
// +--------------------------------+
// |           scrollView           |
// |  +--------------------------+  |
// |  |       contentView        |  |
// |  | (Add custom content here)|  |
// |  +--------------------------+  |
// +--------------------------------+
// |      tertiaryActionButton      |
// +--------------------------------+
// |      primaryActionButton       |
// +--------------------------------+
// |     secondaryActionButton      |
// +--------------------------------+
@interface ButtonStackViewController : UIViewController <ButtonStackConsumer>

// The primary action button.
@property(nonatomic, strong, readonly) ChromeButton* primaryActionButton;

// The secondary action button.
@property(nonatomic, strong, readonly) ChromeButton* secondaryActionButton;

// The tertiary action button.
@property(nonatomic, strong, readonly) ChromeButton* tertiaryActionButton;

// The delegate for button actions.
@property(nonatomic, weak) id<ButtonStackActionDelegate> actionDelegate;

// The configuration for the button stack.
@property(nonatomic, strong, readonly) ButtonStackConfiguration* configuration;

// A container view within the scroll view where subclasses should add their
// custom content.
@property(nonatomic, strong, readonly) UIView* contentView;

// Set to NO to prevent the scroll view from scrolling. Default is YES.
@property(nonatomic, assign) BOOL scrollEnabled;

// Set to NO to prevent the scroll view from showing a vertical scrollbar
// indicator. Must be set before the view is loaded. Default is YES.
@property(nonatomic, assign) BOOL showsVerticalScrollIndicator;

// Controls the scroll view's content inset adjustment behavior.
// Default is UIScrollViewContentInsetAdjustmentAlways.
@property(nonatomic, assign)
    UIScrollViewContentInsetAdjustmentBehavior contentInsetAdjustmentBehavior;

// Controls the addition of the a bottom inset to the contentView.
// Defaults to YES.
@property(nonatomic, assign) BOOL addsContentViewBottomInset;

// Controls the visibility of the gradient view above the action buttons.
// Defaults to YES.
@property(nonatomic, assign) BOOL showsGradientView;

// Designated initializer.
- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

// Initializes the view controller with a default `ButtonStackConfiguration`.
// Prefer using `initWithConfiguration:` when possible.
- (instancetype)init;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Returns YES if the scroll view is scrolled to the bottom.
- (BOOL)isScrolledToBottom;

// Scrolls the view to the end.
- (void)scrollToBottom;

// Returns YES if at least one button is visible.
- (BOOL)hasVisibleButtons;

// Detent that attempts to fit the preferred height of the content. Detent may
// be inactive in some size classes, so it should be used together with at
// least one other detent.
- (UISheetPresentationControllerDetent*)preferredHeightDetent;

// Calculates the preferred height of the content.
// Subclasses should override this method to include the height of any
// additional views they add outside of the `contentView` (e.g., navigation
// bars, headers), while calling `super` to include the base content and button
// stack height.
- (CGFloat)preferredHeightForContent;

@end

#endif  // IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_VIEW_CONTROLLER_H_
