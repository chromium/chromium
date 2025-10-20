// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_VIEW_CONTROLLER_H_
#define IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/button_stack/button_stack_consumer.h"

@class ButtonStackConfiguration;
@class ChromeButton;
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
@property(nonatomic, strong, readonly) UIButton* primaryActionButton;

// The secondary action button.
@property(nonatomic, strong, readonly) UIButton* secondaryActionButton;

// The tertiary action button.
@property(nonatomic, strong, readonly) UIButton* tertiaryActionButton;

// The delegate for button actions.
@property(nonatomic, weak) id<ButtonStackActionDelegate> actionDelegate;

// A container view within the scroll view where subclasses should add their
// custom content.
@property(nonatomic, strong, readonly) UIView* contentView;

// Set to NO to prevent the scroll view from scrolling. Default is YES.
@property(nonatomic, assign) BOOL scrollEnabled;

// Set to NO to prevent the scroll view from showing a vertical scrollbar
// indicator. Must be set before the view is loaded. Default is YES.
@property(nonatomic, assign) BOOL showsVerticalScrollIndicator;

// Sets the custom height for the gradient view above the action buttons.
@property(nonatomic, assign) CGFloat customGradientViewHeight;

// Controls the visibility of the gradient view above the action buttons.
// Defaults to YES.
@property(nonatomic, assign) BOOL showsGradientView;

// Designated initializer.
- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Returns YES if the scroll view is scrolled to the bottom.
- (BOOL)isScrolledToBottom;

// Scrolls the view to the end.
- (void)scrollToBottom;

@end

#endif  // IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_VIEW_CONTROLLER_H_
