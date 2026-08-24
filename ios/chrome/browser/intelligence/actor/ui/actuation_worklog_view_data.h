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

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_DATA_H_
