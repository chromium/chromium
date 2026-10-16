// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"

#import "base/check.h"
#import "base/check_op.h"

namespace {

// Maximum number of actions displayed by an `ActuationInterventionType::kRow`.
constexpr NSUInteger kMaxRowActionCount = 2;

// Maximum number of actions displayed by an
// `ActuationInterventionType::kColumn`.
constexpr NSUInteger kMaxColumnActionCount = 3;

}  // namespace

@implementation ActuationWorklogAccessoryItem

- (instancetype)initWithIcon:(UIImage*)icon
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                  detailText:(NSString*)detailText
                  hasChevron:(BOOL)hasChevron {
  CHECK(title);

  self = [super init];
  if (self) {
    _icon = icon;
    _title = [title copy];
    _subtitle = [subtitle copy];
    _detailText = [detailText copy];
    _hasChevron = hasChevron;
  }
  return self;
}

@end

@implementation ActuationWorklogItem

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                         icon:(UIImage*)icon
                        style:(ActuationWorklogItemStyle)style
                       active:(BOOL)active
                accessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem {
  CHECK(title);
  CHECK(!accessoryItem || style == ActuationWorklogItemStyle::kCard);

  self = [super init];
  if (self) {
    _title = [title copy];
    _subtitle = [subtitle copy];
    _icon = icon;
    _style = style;
    _active = active;
    _accessoryItem = accessoryItem;
  }
  return self;
}

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                         icon:(UIImage*)icon
                        style:(ActuationWorklogItemStyle)style
                       active:(BOOL)active {
  return [self initWithTitle:title
                    subtitle:subtitle
                        icon:icon
                       style:style
                      active:active
               accessoryItem:nil];
}

#pragma mark - Factory Constructors

+ (instancetype)simpleItemWithTitle:(NSString*)title active:(BOOL)active {
  return [[ActuationWorklogItem alloc]
      initWithTitle:title
           subtitle:nil
               icon:nil
              style:ActuationWorklogItemStyle::kSimple
             active:active
      accessoryItem:nil];
}

+ (instancetype)labeledItemWithTitle:(NSString*)title
                            subtitle:(NSString*)subtitle
                                icon:(UIImage*)icon
                              active:(BOOL)active {
  return [[ActuationWorklogItem alloc]
      initWithTitle:title
           subtitle:subtitle
               icon:icon
              style:ActuationWorklogItemStyle::kLabeled
             active:active
      accessoryItem:nil];
}

+ (instancetype)cardItemWithTitle:(NSString*)title
                         subtitle:(NSString*)subtitle
                             icon:(UIImage*)icon
                           active:(BOOL)active {
  return [self cardItemWithTitle:title
                        subtitle:subtitle
                            icon:icon
                          active:active
                   accessoryItem:nil];
}

+ (instancetype)cardItemWithTitle:(NSString*)title
                         subtitle:(NSString*)subtitle
                             icon:(UIImage*)icon
                           active:(BOOL)active
                    accessoryItem:
                        (ActuationWorklogAccessoryItem*)accessoryItem {
  return [[ActuationWorklogItem alloc]
      initWithTitle:title
           subtitle:subtitle
               icon:icon
              style:ActuationWorklogItemStyle::kCard
             active:active
      accessoryItem:accessoryItem];
}

+ (instancetype)freeformItemWithText:(NSString*)text {
  return [[ActuationWorklogItem alloc]
      initWithTitle:text
           subtitle:nil
               icon:nil
              style:ActuationWorklogItemStyle::kFreeform
             active:NO
      accessoryItem:nil];
}

- (instancetype)withActive:(BOOL)active {
  return [[ActuationWorklogItem alloc] initWithTitle:self.title
                                            subtitle:self.subtitle
                                                icon:self.icon
                                               style:self.style
                                              active:active
                                       accessoryItem:self.accessoryItem];
}

@end

@implementation ActuationWorklogChip

- (instancetype)initWithText:(NSString*)text icon:(UIImage*)icon {
  CHECK(text);

  self = [super init];
  if (self) {
    _text = [text copy];
    _icon = icon;
  }
  return self;
}

@end

@implementation ActuationInterventionAction

- (instancetype)initWithIdentifier:(NSInteger)identifier
                             title:(NSString*)title {
  CHECK(title);

  self = [super init];
  if (self) {
    _identifier = identifier;
    _title = [title copy];
  }
  return self;
}

@end

@interface ActuationInterventionData ()

// Designated initializer.
- (instancetype)initWithType:(ActuationInterventionType)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                     actions:(NSArray<ActuationInterventionAction*>*)actions
    NS_DESIGNATED_INITIALIZER;

@end

@implementation ActuationInterventionData

- (instancetype)initWithType:(ActuationInterventionType)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                     actions:(NSArray<ActuationInterventionAction*>*)actions {
  CHECK_GT(actions.count, 0u);
  switch (type) {
    case ActuationInterventionType::kCard:
      CHECK(title);
      CHECK_EQ(actions.count, 1u);
      break;
    case ActuationInterventionType::kRow:
      CHECK_LE(actions.count, kMaxRowActionCount);
      break;
    case ActuationInterventionType::kColumn:
      CHECK_LE(actions.count, kMaxColumnActionCount);
      break;
  }

  self = [super init];
  if (self) {
    _type = type;
    _title = [title copy];
    _subtitle = [subtitle copy];
    _actions = [actions copy];
  }
  return self;
}

#pragma mark - Factory Constructors

+ (instancetype)cardItemWithTitle:(NSString*)title
                         subtitle:(NSString*)subtitle
                           action:(ActuationInterventionAction*)action {
  CHECK(action);
  return [[ActuationInterventionData alloc]
      initWithType:ActuationInterventionType::kCard
             title:title
          subtitle:subtitle
           actions:@[ action ]];
}

+ (instancetype)rowItemWithActions:
    (NSArray<ActuationInterventionAction*>*)actions {
  return [[ActuationInterventionData alloc]
      initWithType:ActuationInterventionType::kRow
             title:nil
          subtitle:nil
           actions:actions];
}

+ (instancetype)columnItemWithActions:
    (NSArray<ActuationInterventionAction*>*)actions {
  return [[ActuationInterventionData alloc]
      initWithType:ActuationInterventionType::kColumn
             title:nil
          subtitle:nil
           actions:actions];
}

@end

@implementation ActuationHeaderContent

- (instancetype)initWithTitle:(NSString*)title subtitle:(NSString*)subtitle {
  self = [super init];
  if (self) {
    _title = [title copy];
    _subtitle = [subtitle copy];
  }
  return self;
}

@end

@implementation ActuationHeaderContentVariants {
  ActuationHeaderContent* _minimizedContent;
  ActuationHeaderContent* _compactContent;
  ActuationHeaderContent* _expandedContent;
}

- (instancetype)initWithMinimizedContent:(ActuationHeaderContent*)minimized
                          compactContent:(ActuationHeaderContent*)compact
                         expandedContent:(ActuationHeaderContent*)expanded {
  self = [super init];
  if (self) {
    _minimizedContent = minimized;
    _compactContent = compact;
    _expandedContent = expanded;
  }
  return self;
}

- (ActuationHeaderContent*)contentForDisplayMode:
    (ActuationWorklogDisplayMode)mode {
  switch (mode) {
    case ActuationWorklogDisplayModeMinimized:
      return _minimizedContent;
    case ActuationWorklogDisplayModeCompact:
      return _compactContent;
    case ActuationWorklogDisplayModeExpanded:
      return _expandedContent;
  }
}

@end

@interface ActuationHeaderItem ()

- (instancetype)initWithStyle:(ActuationHeaderItemStyle)style
                         icon:(UIImage*)icon
                        title:(NSString*)title
      accessibilityIdentifier:(NSString*)accessibilityIdentifier
                       action:(UIAction*)action
                         menu:(UIMenu*)menu NS_DESIGNATED_INITIALIZER;

@end

@implementation ActuationHeaderItem

+ (instancetype)itemWithIcon:(UIImage*)icon
                       title:(NSString*)title
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
                      action:(UIAction*)action {
  CHECK(icon);
  CHECK(action);
  return [[self alloc] initWithStyle:ActuationHeaderItemStyleIcon
                                icon:icon
                               title:title
             accessibilityIdentifier:accessibilityIdentifier
                              action:action
                                menu:nil];
}

+ (instancetype)itemWithIcon:(UIImage*)icon
                       title:(NSString*)title
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
                        menu:(UIMenu*)menu {
  CHECK(icon);
  CHECK(menu);
  return [[self alloc] initWithStyle:ActuationHeaderItemStyleIcon
                                icon:icon
                               title:title
             accessibilityIdentifier:accessibilityIdentifier
                              action:nil
                                menu:menu];
}

+ (instancetype)callToActionWithTitle:(NSString*)title
              accessibilityIdentifier:(NSString*)accessibilityIdentifier
                               action:(UIAction*)action {
  CHECK(action);
  return [[self alloc] initWithStyle:ActuationHeaderItemStyleCallToAction
                                icon:nil
                               title:title
             accessibilityIdentifier:accessibilityIdentifier
                              action:action
                                menu:nil];
}

#pragma mark - Private

- (instancetype)initWithStyle:(ActuationHeaderItemStyle)style
                         icon:(UIImage*)icon
                        title:(NSString*)title
      accessibilityIdentifier:(NSString*)accessibilityIdentifier
                       action:(UIAction*)action
                         menu:(UIMenu*)menu {
  CHECK(title);

  self = [super init];
  if (self) {
    _style = style;
    _icon = icon;
    _title = [title copy];
    _accessibilityIdentifier = [accessibilityIdentifier copy];
    _action = action;
    _menu = menu;
  }
  return self;
}

@end
