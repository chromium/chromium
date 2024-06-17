// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_SECONDARY_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_SECONDARY_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view.h"

@class ToolbarButtonFactory;

// View for the secondary part of the adaptive toolbar. It is the part
// containing the controls displayed only on specific size classes.
@interface SecondaryToolbarView : UIView<AdaptiveToolbarView>

// StackView containing the navigation buttons from `ToolbarButtons`.
@property(nonatomic, strong, readonly) UIStackView* buttonStackView;
// Separator below the location bar. Used when collapsed above the keyboard.
@property(nonatomic, strong, readonly) UIView* bottomSeparator;

// Constraint for the top of the location bar.
@property(nonatomic, strong) NSLayoutConstraint* locationBarTopConstraint;
// Constraint for the bottom of the location bar. Used to move the location bar
// above the keyboard.
@property(nonatomic, strong) NSLayoutConstraint* locationBarKeyboardConstraint;

// Initialize this View with the button `factory`.
- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)factory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Makes the toolbar translucent.
- (void)makeTranslucent;

// Makes the toolbar opaque.
- (void)makeOpaque;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_SECONDARY_TOOLBAR_VIEW_H_
