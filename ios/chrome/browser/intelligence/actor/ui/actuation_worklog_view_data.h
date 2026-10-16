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
  // Freeform text-only style (e.g. a question or clarification) spanning the
  // full width, without any timeline decoration (no dot, icon or connector).
  kFreeform,
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

// Creates a freeform text-only item without timeline decoration. `text` is
// exposed through `title`.
+ (instancetype)freeformItemWithText:(NSString*)text;

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

// Presentation modes of the actuation worklog.
typedef NS_ENUM(NSInteger, ActuationWorklogDisplayMode) {
  // Only the header is visible.
  ActuationWorklogDisplayModeMinimized,
  // The header and the current step are visible.
  ActuationWorklogDisplayModeCompact,
  // The header and the full timeline are visible.
  ActuationWorklogDisplayModeExpanded,
};

// Visual layouts for an interactive intervention in the actuation worklog.
enum class ActuationInterventionType {
  // Card with `title`, optional `subtitle`, and a single action button.
  kCard,
  // Row of one or two action buttons, without `title` or `subtitle`.
  kRow,
  // Column of one to three action buttons, without `title` or `subtitle`.
  kColumn,
};

// View data object describing an action of an `ActuationInterventionData`.
@interface ActuationInterventionAction : NSObject

// Caller-defined identifier, passed back when the action is triggered.
@property(nonatomic, assign, readonly) NSInteger identifier;

// Title of the button triggering the action.
@property(nonatomic, copy, readonly) NSString* title;

// TODO(crbug.com/573145076): Add an optional `icon` for `columnItem`.

// Designated initializer.
- (instancetype)initWithIdentifier:(NSInteger)identifier
                             title:(NSString*)title NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// View data object describing an intervention in the actuation worklog.
@interface ActuationInterventionData : NSObject

// The visual layout type of the intervention.
@property(nonatomic, assign, readonly) ActuationInterventionType type;

// Optional title text (mandatory if `type == kCard`).
@property(nonatomic, copy, readonly) NSString* title;

// Optional subtitle text (used if `type == kCard`).
@property(nonatomic, copy, readonly) NSString* subtitle;

// Actions of the intervention, in display order. Never empty.
@property(nonatomic, copy, readonly)
    NSArray<ActuationInterventionAction*>* actions;

- (instancetype)init NS_UNAVAILABLE;

#pragma mark - Factory Constructors

// Creates a card layout intervention with a single `action`.
+ (instancetype)cardItemWithTitle:(NSString*)title
                         subtitle:(NSString*)subtitle
                           action:(ActuationInterventionAction*)action;

// Creates an intervention with a row of one or two `actions`, in display order.
+ (instancetype)rowItemWithActions:
    (NSArray<ActuationInterventionAction*>*)actions;

// Creates an intervention with a column of one to three `actions`, in display
// order.
+ (instancetype)columnItemWithActions:
    (NSArray<ActuationInterventionAction*>*)actions;

@end

// View data object describing the text displayed in the actuation header.
@interface ActuationHeaderContent : NSObject

// Main title text.
@property(nonatomic, copy, readonly) NSString* title;

// Optional subtitle text displayed below `title`.
@property(nonatomic, copy, readonly) NSString* subtitle;

// Designated initializer.
- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// View data object describing the header content to display in each
// `ActuationWorklogDisplayMode`.
@interface ActuationHeaderContentVariants : NSObject

// Designated initializer.
- (instancetype)initWithMinimizedContent:(ActuationHeaderContent*)minimized
                          compactContent:(ActuationHeaderContent*)compact
                         expandedContent:(ActuationHeaderContent*)expanded
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns the content to display in `mode`.
- (ActuationHeaderContent*)contentForDisplayMode:
    (ActuationWorklogDisplayMode)mode;

@end

// Visual style of an `ActuationHeaderItem`.
typedef NS_ENUM(NSInteger, ActuationHeaderItemStyle) {
  // Icon-only button. `title` is used as the accessibility label.
  ActuationHeaderItemStyleIcon,
  // Prominent text-only capsule button displaying `title`.
  ActuationHeaderItemStyleCallToAction,
};

// View data object describing a button in the actuation header. The button will
// trigger `action` or presents `menu` on tap; exactly one of them is non-nil.
@interface ActuationHeaderItem : NSObject

// Visual style of the button, determined by the factory used.
@property(nonatomic, assign, readonly) ActuationHeaderItemStyle style;

// Icon displayed in the button. Nil for
// `ActuationHeaderItemStyleCallToAction` items.
@property(nonatomic, strong, readonly) UIImage* icon;

// Title of the item. Displayed as the button text for
// `ActuationHeaderItemStyleCallToAction` items, otherwise used as the
// accessibility label of the icon-only button.
@property(nonatomic, copy, readonly) NSString* title;

// Optional accessibility identifier of the button.
@property(nonatomic, copy, readonly) NSString* accessibilityIdentifier;

// Action triggered on tap. Nil when `menu` is set.
@property(nonatomic, strong, readonly) UIAction* action;

// Menu presented on tap. Nil when `action` is set.
@property(nonatomic, strong, readonly) UIMenu* menu;

// Creates an `ActuationHeaderItemStyleIcon` item triggering `action` on tap.
+ (instancetype)itemWithIcon:(UIImage*)icon
                       title:(NSString*)title
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
                      action:(UIAction*)action;

// Creates an `ActuationHeaderItemStyleIcon` item presenting `menu` on tap.
+ (instancetype)itemWithIcon:(UIImage*)icon
                       title:(NSString*)title
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
                        menu:(UIMenu*)menu;

// Creates an `ActuationHeaderItemStyleCallToAction` item displaying `title`
// and triggering `action` on tap.
+ (instancetype)callToActionWithTitle:(NSString*)title
              accessibilityIdentifier:(NSString*)accessibilityIdentifier
                               action:(UIAction*)action;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_DATA_H_
