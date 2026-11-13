// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/ui/intelligence_header_item.h"

#import "base/check.h"

@interface IntelligenceHeaderItem ()

- (instancetype)initWithStyle:(IntelligenceHeaderItemStyle)style
                         icon:(UIImage*)icon
                        title:(NSString*)title
      accessibilityIdentifier:(NSString*)accessibilityIdentifier
                       action:(UIAction*)action
                         menu:(UIMenu*)menu NS_DESIGNATED_INITIALIZER;

@end

@implementation IntelligenceHeaderItem

+ (instancetype)itemWithIcon:(UIImage*)icon
                       title:(NSString*)title
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
                      action:(UIAction*)action {
  CHECK(icon);
  CHECK(action);
  return [[self alloc] initWithStyle:IntelligenceHeaderItemStyle::kIcon
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
  return [[self alloc] initWithStyle:IntelligenceHeaderItemStyle::kIcon
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
  return [[self alloc] initWithStyle:IntelligenceHeaderItemStyle::kCallToAction
                                icon:nil
                               title:title
             accessibilityIdentifier:accessibilityIdentifier
                              action:action
                                menu:nil];
}

#pragma mark - Private

- (instancetype)initWithStyle:(IntelligenceHeaderItemStyle)style
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
