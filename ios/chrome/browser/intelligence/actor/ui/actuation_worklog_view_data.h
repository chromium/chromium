// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_DATA_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_DATA_H_

#import <UIKit/UIKit.h>

// Supported layout styles for the actuation worklog item.
enum class ActuationWorklogItemStyle {
  // Simple style with a dot indicator on the left and a title.
  kSimple,
  // Labeled style with an icon in a circle, and a title/subtitle next to it.
  kLabeled,
  // Card style with an icon in a circle, and the title/subtitle inside a card
  // container. Optionally contains an `ActuationWorklogAccessoryItem`.
  kCard,
};

// View data object describing a nested accessory card inside an actuation
// worklog card item.
@interface ActuationWorklogAccessoryItem : NSObject

// Icon image.
@property(nonatomic, strong, readonly) UIImage* icon;

// Primary title text.
@property(nonatomic, copy, readonly) NSString* title;

// Optional subtitle text.
@property(nonatomic, copy, readonly) NSString* subtitle;

// Optional detail text (e.g. expiration date).
@property(nonatomic, copy, readonly) NSString* detailText;

// Whether to show a trailing chevron.
@property(nonatomic, assign, readonly) BOOL hasChevron;

// Designated initializer.
- (instancetype)initWithIcon:(UIImage*)icon
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                  detailText:(NSString*)detailText
                  hasChevron:(BOOL)hasChevron NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// View data object describing a node in the actuation worklog.
@interface ActuationWorklogItem : NSObject

// Title text.
@property(nonatomic, copy, readonly) NSString* title;

// Optional subtitle text.
@property(nonatomic, copy, readonly) NSString* subtitle;

// Optional icon image.
@property(nonatomic, strong, readonly) UIImage* icon;

// True if this node is currently active.
@property(nonatomic, assign, readonly, getter=isActive) BOOL active;

// The layout style of this node.
@property(nonatomic, assign, readonly) ActuationWorklogItemStyle style;

// Optional accessory item (only used when style ==
// ActuationWorklogItemStyle::kCard).
@property(nonatomic, strong, readonly)
    ActuationWorklogAccessoryItem* accessoryItem;

// Designated initializer.
- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                         icon:(UIImage*)icon
                        style:(ActuationWorklogItemStyle)style
                       active:(BOOL)active
                accessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                         icon:(UIImage*)icon
                        style:(ActuationWorklogItemStyle)style
                       active:(BOOL)active;

- (instancetype)init NS_UNAVAILABLE;

#pragma mark - Factory Constructors

// Creates a simple timeline step with a dot indicator.
+ (instancetype)simpleItemWithTitle:(NSString*)title active:(BOOL)active;

// Creates a labeled timeline step.
+ (instancetype)labeledItemWithTitle:(NSString*)title
                            subtitle:(NSString*)subtitle
                                icon:(UIImage*)icon
                              active:(BOOL)active;

// Creates a card layout step without an accessory item.
+ (instancetype)cardItemWithTitle:(NSString*)title
                         subtitle:(NSString*)subtitle
                             icon:(UIImage*)icon
                           active:(BOOL)active;

// Creates a card layout step with a nested accessory item.
+ (instancetype)cardItemWithTitle:(NSString*)title
                         subtitle:(NSString*)subtitle
                             icon:(UIImage*)icon
                           active:(BOOL)active
                    accessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem;

#pragma mark - Helpers

// Returns a copy of the item with the active state updated.
- (instancetype)withActive:(BOOL)active;

@end

// View data object describing an actor tool chip in the actuation worklog.
@interface ActuationWorklogChip : NSObject

// The text displayed on the chip.
@property(nonatomic, copy, readonly) NSString* text;

// The icon displayed on the chip.
@property(nonatomic, strong, readonly) UIImage* icon;

// Designated initializer.
- (instancetype)initWithText:(NSString*)text
                        icon:(UIImage*)icon NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// Visual layouts for an interactive intervention in the actuation worklog.
enum class ActuationInterventionType {
  // Card with `title`, optional `subtitle`, and a primary action button.
  kCard,
  // Standalone full-width primary action button.
  kSingleButton,
  // Standalone, side-by-side secondary and primary action buttons.
  kDualButton,
};

// User action triggered on an active worklog intervention.
enum class ActuationInterventionAction {
  // The primary confirmation/continuation action.
  kPrimary,
  // The secondary cancellation/alternative action.
  kSecondary,
};

// View data object describing an intervention in the actuation worklog.
@interface ActuationInterventionData : NSObject

// The visual layout type of the intervention.
@property(nonatomic, assign, readonly) ActuationInterventionType type;

// Optional title text (mandatory if `type == kCard`).
@property(nonatomic, copy, readonly) NSString* title;

// Optional subtitle text (used if `type == kCard`).
@property(nonatomic, copy, readonly) NSString* subtitle;

// Mandatory primary action button title.
@property(nonatomic, copy, readonly) NSString* primaryButtonText;

// Optional secondary action button title (mandatory if `type == kDualButton`).
@property(nonatomic, copy, readonly) NSString* secondaryButtonText;

- (instancetype)init NS_UNAVAILABLE;

#pragma mark - Factory Constructors

// Creates a card layout intervention with a single action button.
+ (instancetype)cardItemWithTitle:(NSString*)title
                         subtitle:(NSString*)subtitle
                primaryButtonText:(NSString*)primaryButtonText;

// Creates a single-button layout intervention.
+ (instancetype)singleButtonItemWithPrimaryButtonText:
    (NSString*)primaryButtonText;

// Creates a dual-button layout intervention.
+ (instancetype)dualButtonItemWithPrimaryButtonText:(NSString*)primaryButtonText
                                secondaryButtonText:
                                    (NSString*)secondaryButtonText;

@end

// View data object describing a button in the actuation header. The button will
// trigger `action` or presents `menu` on tap; exactly one of them is non-nil.
@interface ActuationHeaderItem : NSObject

// Icon displayed in the button.
@property(nonatomic, strong, readonly) UIImage* icon;

// Title of the item, used as the accessibility label of the icon-only button.
@property(nonatomic, copy, readonly) NSString* title;

// Optional accessibility identifier of the button.
@property(nonatomic, copy, readonly) NSString* accessibilityIdentifier;

// Action triggered on tap. Nil when `menu` is set.
@property(nonatomic, strong, readonly) UIAction* action;

// Menu presented on tap. Nil when `action` is set.
@property(nonatomic, strong, readonly) UIMenu* menu;

// Creates an item triggering `action` on tap.
- (instancetype)initWithIcon:(UIImage*)icon
                       title:(NSString*)title
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
                      action:(UIAction*)action NS_DESIGNATED_INITIALIZER;

// Creates an item presenting `menu` on tap.
- (instancetype)initWithIcon:(UIImage*)icon
                       title:(NSString*)title
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
                        menu:(UIMenu*)menu NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_DATA_H_
