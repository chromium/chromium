// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

@class IntelligenceHeaderItem;

// Vertical padding density of an `IntelligenceHeaderView`.
enum class IntelligenceHeaderDensity {
  // Standard padding.
  kRegular,
  // Reduced padding, for space-constrained presentations.
  kCondensed,
};

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

// Main title text.
@property(nonatomic, copy) NSString* title;

// Subtitle text displayed below `title`.
@property(nonatomic, copy) NSString* subtitle;

// Whether the activity indicator ring around the leading icon is animating.
// Default is NO (static logo).
@property(nonatomic, assign, getter=isLoading) BOOL loading;

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

// Resets the header state, clearing all content and accessory properties.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_VIEW_H_
