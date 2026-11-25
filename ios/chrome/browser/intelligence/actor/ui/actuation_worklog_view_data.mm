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
                accessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem {
  CHECK(title);
  CHECK(!accessoryItem || style == ActuationWorklogItemStyle::kCard);

  self = [super init];
  if (self) {
    _title = [title copy];
    _subtitle = [subtitle copy];
    _icon = icon;
    _style = style;
    _accessoryItem = accessoryItem;
  }
  return self;
}

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                         icon:(UIImage*)icon
                        style:(ActuationWorklogItemStyle)style {
  return [self initWithTitle:title
                    subtitle:subtitle
                        icon:icon
                       style:style
               accessoryItem:nil];
}

#pragma mark - Factory Constructors

+ (instancetype)simpleItemWithTitle:(NSString*)title {
  return [[ActuationWorklogItem alloc]
      initWithTitle:title
           subtitle:nil
               icon:nil
              style:ActuationWorklogItemStyle::kSimple
      accessoryItem:nil];
}

+ (instancetype)labeledItemWithTitle:(NSString*)title
                            subtitle:(NSString*)subtitle
                                icon:(UIImage*)icon {
  return [[ActuationWorklogItem alloc]
      initWithTitle:title
           subtitle:subtitle
               icon:icon
              style:ActuationWorklogItemStyle::kLabeled
      accessoryItem:nil];
}

+ (instancetype)cardItemWithTitle:(NSString*)title
                         subtitle:(NSString*)subtitle
                             icon:(UIImage*)icon {
  return [self cardItemWithTitle:title
                        subtitle:subtitle
                            icon:icon
                   accessoryItem:nil];
}

+ (instancetype)cardItemWithTitle:(NSString*)title
                         subtitle:(NSString*)subtitle
                             icon:(UIImage*)icon
                    accessoryItem:
                        (ActuationWorklogAccessoryItem*)accessoryItem {
  return [[ActuationWorklogItem alloc]
      initWithTitle:title
           subtitle:subtitle
               icon:icon
              style:ActuationWorklogItemStyle::kCard
      accessoryItem:accessoryItem];
}

+ (instancetype)freeformItemWithText:(NSString*)text {
  return [[ActuationWorklogItem alloc]
      initWithTitle:text
           subtitle:nil
               icon:nil
              style:ActuationWorklogItemStyle::kFreeform
      accessoryItem:nil];
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

- (instancetype)initWithIdentifier:(InterventionActionIdentifier)identifier
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

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                  primaryItem:(IntelligenceHeaderItem*)primaryItem {
  self = [super init];
  if (self) {
    _title = [title copy];
    _subtitle = [subtitle copy];
    _primaryItem = primaryItem;
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

@implementation ActuationWorklogEntry

- (instancetype)initWithItem:(ActuationWorklogItem*)item
                        chip:(ActuationWorklogChip*)chip {
  CHECK(item);
  self = [super init];
  if (self) {
    _item = item;
    _chip = chip;
  }
  return self;
}

@end

@implementation ActuationWorklogState

- (instancetype)
    initWithActuationActive:(BOOL)actuationActive
              headerContent:(ActuationHeaderContentVariants*)headerContent
                    entries:(NSArray<ActuationWorklogEntry*>*)entries
               intervention:(ActuationInterventionData*)intervention
                suggestions:(NSArray<ActorFormSuggestion*>*)suggestions
    selectedSuggestionIndex:(NSInteger)selectedSuggestionIndex
           suggestionsEntry:(ActuationWorklogEntry*)suggestionsEntry {
  CHECK(headerContent);
  CHECK(entries);
  CHECK_EQ(!suggestions, !suggestionsEntry);
  if (suggestions) {
    CHECK_GT(suggestions.count, 0u);
    CHECK_GE(selectedSuggestionIndex, 0);
    CHECK_LT(static_cast<NSUInteger>(selectedSuggestionIndex),
             suggestions.count);
  } else {
    CHECK_EQ(selectedSuggestionIndex, NSNotFound);
  }
  self = [super init];
  if (self) {
    _actuationActive = actuationActive;
    _headerContent = headerContent;
    _entries = [entries copy];
    _intervention = intervention;
    _suggestions = [suggestions copy];
    _selectedSuggestionIndex = selectedSuggestionIndex;
    _suggestionsEntry = suggestionsEntry;
  }
  return self;
}

@end
