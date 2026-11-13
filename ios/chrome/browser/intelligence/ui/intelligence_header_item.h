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

// View data object describing an accessory button in an
// `IntelligenceHeaderView`. The button triggers `action` or presents `menu` on
// tap; exactly one of them is non-nil.
@interface IntelligenceHeaderItem : NSObject

// Visual style of the button, determined by the factory used.
@property(nonatomic, assign, readonly) IntelligenceHeaderItemStyle style;

// Icon displayed in the button. Nil for
// `IntelligenceHeaderItemStyle::kCallToAction` items.
@property(nonatomic, strong, readonly) UIImage* icon;

// Title of the item. Displayed as the button text for
// `IntelligenceHeaderItemStyle::kCallToAction` items, otherwise used as the
// accessibility label of the icon-only button.
@property(nonatomic, copy, readonly) NSString* title;

// Optional accessibility identifier of the button.
@property(nonatomic, copy, readonly) NSString* accessibilityIdentifier;

// Action triggered on tap. Nil when `menu` is set.
@property(nonatomic, strong, readonly) UIAction* action;

// Menu presented on tap. Nil when `action` is set.
@property(nonatomic, strong, readonly) UIMenu* menu;

// Creates an `IntelligenceHeaderItemStyle::kIcon` item triggering `action` on
// tap.
+ (instancetype)itemWithIcon:(UIImage*)icon
                       title:(NSString*)title
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
                      action:(UIAction*)action;

// Creates an `IntelligenceHeaderItemStyle::kIcon` item presenting `menu` on
// tap.
+ (instancetype)itemWithIcon:(UIImage*)icon
                       title:(NSString*)title
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
                        menu:(UIMenu*)menu;

// Creates an `IntelligenceHeaderItemStyle::kCallToAction` item displaying
// `title` and triggering `action` on tap.
+ (instancetype)callToActionWithTitle:(NSString*)title
              accessibilityIdentifier:(NSString*)accessibilityIdentifier
                               action:(UIAction*)action;

+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_UI_INTELLIGENCE_HEADER_ITEM_H_
