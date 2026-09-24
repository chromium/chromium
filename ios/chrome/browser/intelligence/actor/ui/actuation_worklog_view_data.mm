// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"

#import "base/check.h"

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

@interface ActuationInterventionData ()

// Designated initializer.
- (instancetype)initWithType:(ActuationInterventionType)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
           primaryButtonText:(NSString*)primaryButtonText
         secondaryButtonText:(NSString*)secondaryButtonText
    NS_DESIGNATED_INITIALIZER;

@end

@implementation ActuationInterventionData

- (instancetype)initWithType:(ActuationInterventionType)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
           primaryButtonText:(NSString*)primaryButtonText
         secondaryButtonText:(NSString*)secondaryButtonText {
  CHECK(primaryButtonText);
  switch (type) {
    case ActuationInterventionType::kCard:
      CHECK(title);
      break;
    case ActuationInterventionType::kSingleButton:
      break;
    case ActuationInterventionType::kDualButton:
      CHECK(secondaryButtonText);
      break;
  }

  self = [super init];
  if (self) {
    _type = type;
    _title = [title copy];
    _subtitle = [subtitle copy];
    _primaryButtonText = [primaryButtonText copy];
    _secondaryButtonText = [secondaryButtonText copy];
  }
  return self;
}

#pragma mark - Factory Constructors

+ (instancetype)cardItemWithTitle:(NSString*)title
                         subtitle:(NSString*)subtitle
                primaryButtonText:(NSString*)primaryButtonText {
  return [[ActuationInterventionData alloc]
             initWithType:ActuationInterventionType::kCard
                    title:title
                 subtitle:subtitle
        primaryButtonText:primaryButtonText
      secondaryButtonText:nil];
}

+ (instancetype)singleButtonItemWithPrimaryButtonText:
    (NSString*)primaryButtonText {
  return [[ActuationInterventionData alloc]
             initWithType:ActuationInterventionType::kSingleButton
                    title:nil
                 subtitle:nil
        primaryButtonText:primaryButtonText
      secondaryButtonText:nil];
}

+ (instancetype)dualButtonItemWithPrimaryButtonText:(NSString*)primaryButtonText
                                secondaryButtonText:
                                    (NSString*)secondaryButtonText {
  return [[ActuationInterventionData alloc]
             initWithType:ActuationInterventionType::kDualButton
                    title:nil
                 subtitle:nil
        primaryButtonText:primaryButtonText
      secondaryButtonText:secondaryButtonText];
}

@end
