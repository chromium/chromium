// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/ui/intelligence_header_configuration.h"

@class IntelligenceHeaderItem;
@class IntelligenceHeaderView;

// Delegate protocol receiving user interaction events from an
// `IntelligenceHeaderView`.
@protocol IntelligenceHeaderViewDelegate <NSObject>

// Called when the user taps an action-backed accessory item with `identifier`.
- (void)headerView:(IntelligenceHeaderView*)headerView
    didTriggerItemWithIdentifier:(NSString*)identifier;

@end

// Reusable intelligence header view displaying a leading icon (with an optional
// animated gradient ring), a title/subtitle stack, and trailing accessory
// buttons.
//
// One secondary item:
// ┌────────────────────────────────────────────────────┐
// │ (*)  Title                   [secondary] [primary] │
// │      Subtitle (optional)                           │
// └────────────────────────────────────────────────────┘
//
// Two or more icon secondary items:
// ┌────────────────────────────────────────────────────┐
// │ (*)  Title                   ( [a] [b] ) [primary] │
// │      Subtitle (optional)                           │
// └────────────────────────────────────────────────────┘
//
// Call-to-action secondary item:
// ┌────────────────────────────────────────────────────┐
// │ (*)  Title                  [ Call to action ] [x] │
// │      Subtitle (optional)                           │
// └────────────────────────────────────────────────────┘
// (*) = Icon, [x] = Accessory button, ( [a] [b] ) = Grouped capsule,
// [ Call to action ] = Blue text capsule
@interface IntelligenceHeaderView : UIView

// Delegate receiving accessory item tap events.
@property(nonatomic, weak) id<IntelligenceHeaderViewDelegate> delegate;

// Icon displayed in the leading slot. When `nil`, defaults to the Gemini logo.
@property(nonatomic, strong) UIImage* leadingIcon;

// Main title text.
@property(nonatomic, copy) NSString* title;

// Subtitle text displayed below `title`.
@property(nonatomic, copy) NSString* subtitle;

// Activity state controlling the indicator ring around `leadingIcon`. Default
// is `IntelligenceHeaderActivity::kIdle`.
@property(nonatomic, assign) IntelligenceHeaderActivity activity;

// Vertical padding around the content. Default is
// `IntelligenceHeaderDensity::kRegular`. Not affected by `reset`.
@property(nonatomic, assign) IntelligenceHeaderDensity density;

// Primary accessory button, rendered according to its `style`. Setting nil
// removes the button from the header.
@property(nonatomic, strong) IntelligenceHeaderItem* primaryItem;

// Items displayed before `primaryItem`, in the given order. Two or more items
// that are all `IntelligenceHeaderItemStyle::kIcon` are grouped in a capsule;
// any other composition renders each item as a standalone button according to
// its `style`. Setting nil or an empty array removes them from the header.
@property(nonatomic, copy) NSArray<IntelligenceHeaderItem*>* secondaryItems;

- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Applies `configuration` atomically (`configuration` must be non-nil). Prefer
// this for full state updates; the property setters are convenience helpers for
// single-field tweaks.
- (void)applyConfiguration:(IntelligenceHeaderConfiguration*)configuration;

// Resets the header state, clearing all content and accessory properties.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_VIEW_H_
