// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_ITEM_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_ITEM_H_

#import <UIKit/UIKit.h>

// Visual style of an `IntelligenceHeaderItem`.
enum class IntelligenceHeaderItemStyle {
  // Icon-only button. `title` is used as the accessibility label.
  kIcon,
  // Prominent text-only capsule button displaying `title`.
  kCallToAction,
};

// Immutable view data object describing an accessory button in an
// `IntelligenceHeaderView`. Items are matched across updates by `identifier`;
// when `menu` is set the button presents `menu` on tap, otherwise tapping
// notifies `IntelligenceHeaderViewDelegate` with `identifier`.
@interface IntelligenceHeaderItem : NSObject

// Visual style of the button, determined by the factory used.
@property(nonatomic, assign, readonly) IntelligenceHeaderItemStyle style;

// Non-empty stable identifier used to match buttons across updates and passed
// to `IntelligenceHeaderViewDelegate` when an action-backed item is tapped.
@property(nonatomic, copy, readonly) NSString* identifier;

// Icon displayed in the button. Nil for
// `IntelligenceHeaderItemStyle::kCallToAction` items.
@property(nonatomic, strong, readonly) UIImage* icon;

// Title of the item. Displayed as the button text for
// `IntelligenceHeaderItemStyle::kCallToAction` items, otherwise used as the
// accessibility label of the icon-only button.
@property(nonatomic, copy, readonly) NSString* title;

// Optional accessibility identifier of the button.
@property(nonatomic, copy, readonly) NSString* accessibilityIdentifier;

// Optional menu presented on tap instead of triggering the delegate action.
@property(nonatomic, strong, readonly) UIMenu* menu;

// Creates an `IntelligenceHeaderItemStyle::kIcon` item routing taps with
// `identifier` to the header view's delegate.
+ (instancetype)itemWithIdentifier:(NSString*)identifier
                              icon:(UIImage*)icon
                             title:(NSString*)title
           accessibilityIdentifier:(NSString*)accessibilityIdentifier;

// Creates an `IntelligenceHeaderItemStyle::kIcon` item presenting `menu` on
// tap.
+ (instancetype)itemWithIdentifier:(NSString*)identifier
                              icon:(UIImage*)icon
                             title:(NSString*)title
           accessibilityIdentifier:(NSString*)accessibilityIdentifier
                              menu:(UIMenu*)menu;

// Creates an `IntelligenceHeaderItemStyle::kCallToAction` item displaying
// `title` and routing taps with `identifier` to the header view's delegate.
+ (instancetype)callToActionWithIdentifier:(NSString*)identifier
                                     title:(NSString*)title
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier;

+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_ITEM_H_
