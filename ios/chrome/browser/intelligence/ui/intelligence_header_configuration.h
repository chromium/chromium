// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@class IntelligenceHeaderItem;

// Activity state of an `IntelligenceHeaderView`, controlling the indicator
// around the leading icon.
enum class IntelligenceHeaderActivity {
  // Static leading icon without an activity ring.
  kIdle,
  // Animated gradient ring around the leading icon.
  kActive,
};

// Vertical padding density of an `IntelligenceHeaderView`.
enum class IntelligenceHeaderDensity {
  // Standard padding.
  kRegular,
  // Reduced padding, for space-constrained presentations.
  kCondensed,
};

// Immutable snapshot of the content, activity, density, and accessories
// displayed by an `IntelligenceHeaderView`.
@interface IntelligenceHeaderConfiguration : NSObject

// Icon displayed in the leading slot. When `nil`, defaults to the Gemini logo.
@property(nonatomic, strong, readonly) UIImage* leadingIcon;

// Main title text.
@property(nonatomic, copy, readonly) NSString* title;

// Subtitle text displayed below `title`.
@property(nonatomic, copy, readonly) NSString* subtitle;

// Activity state controlling the indicator ring around `leadingIcon`.
@property(nonatomic, assign, readonly) IntelligenceHeaderActivity activity;

// Vertical padding around the content.
@property(nonatomic, assign, readonly) IntelligenceHeaderDensity density;

// Primary accessory button, rendered according to its `style`.
@property(nonatomic, strong, readonly) IntelligenceHeaderItem* primaryItem;

// Items displayed before `primaryItem`, in display order.
@property(nonatomic, copy, readonly)
    NSArray<IntelligenceHeaderItem*>* secondaryItems;

// Initializes a header configuration. `leadingIcon` defaults to the Gemini logo
// when `nil`; `title`, `subtitle`, `primaryItem`, and `secondaryItems` are
// optional and hide their respective elements when `nil` or empty.
- (instancetype)initWithLeadingIcon:(UIImage*)leadingIcon
                              title:(NSString*)title
                           subtitle:(NSString*)subtitle
                           activity:(IntelligenceHeaderActivity)activity
                            density:(IntelligenceHeaderDensity)density
                        primaryItem:(IntelligenceHeaderItem*)primaryItem
                     secondaryItems:
                         (NSArray<IntelligenceHeaderItem*>*)secondaryItems
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_CONFIGURATION_H_
