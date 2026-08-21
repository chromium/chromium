// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_TASK_CARD_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_TASK_CARD_VIEW_H_

#import <UIKit/UIKit.h>

@class ActuationTaskCardView;

// Delegate protocol for user interaction events on ActuationTaskCardView.
@protocol ActuationTaskCardViewDelegate <NSObject>

// Called when the primary action button at the bottom is tapped.
- (void)taskCardViewDidTapActionButton:(ActuationTaskCardView*)view;

// Called when the card's collapsed state changes via user interaction.
- (void)taskCardView:(ActuationTaskCardView*)view
    didChangeCollapsedState:(BOOL)isCollapsed;

@end

// Interactive card view component for agentic task interaction.
@interface ActuationTaskCardView : UIView

// Delegate receiving interaction events.
@property(nonatomic, weak) id<ActuationTaskCardViewDelegate> delegate;

// Mandatory bold title text.
@property(nonatomic, copy) NSString* title;

// Mandatory action button title text.
@property(nonatomic, copy) NSString* buttonTitle;

// Optional top-left header icon image.
@property(nonatomic, strong) UIImage* headerIcon;

// Optional subtitle text below the top header row.
@property(nonatomic, copy) NSString* subtitle;

// Enables or disables the top-right collapsible chevron button.
@property(nonatomic, assign, readonly, getter=isCollapsible) BOOL collapsible;

// Collapsed state of the card.
@property(nonatomic, assign, getter=isCollapsed) BOOL collapsed;

// Enables or disables the action button.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// Designated initializer taking mandatory fields and collapsibility flag.
- (instancetype)initWithTitle:(NSString*)title
                  buttonTitle:(NSString*)buttonTitle
                  collapsible:(BOOL)collapsible NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Sets or clears the background gradient colors. Passing nil for both resets
// the view background to the default background color.
- (void)setBackgroundGradientStartColor:(UIColor*)startColor
                               endColor:(UIColor*)endColor;

// Sets the collapsed state with optional smooth animation.
- (void)setCollapsed:(BOOL)collapsed animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_TASK_CARD_VIEW_H_
