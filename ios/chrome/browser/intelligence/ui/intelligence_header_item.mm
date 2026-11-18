// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/ui/intelligence_header_item.h"

#import "base/check.h"
#import "base/check_op.h"

@interface IntelligenceHeaderItem ()

- (instancetype)initWithStyle:(IntelligenceHeaderItemStyle)style
                   identifier:(NSString*)identifier
                         icon:(UIImage*)icon
                        title:(NSString*)title
      accessibilityIdentifier:(NSString*)accessibilityIdentifier
                         menu:(UIMenu*)menu NS_DESIGNATED_INITIALIZER;

@end

@implementation IntelligenceHeaderItem

+ (instancetype)itemWithIdentifier:(NSString*)identifier
                              icon:(UIImage*)icon
                             title:(NSString*)title
           accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  CHECK(icon);
  return [[self alloc] initWithStyle:IntelligenceHeaderItemStyle::kIcon
                          identifier:identifier
                                icon:icon
                               title:title
             accessibilityIdentifier:accessibilityIdentifier
                                menu:nil];
}

+ (instancetype)itemWithIdentifier:(NSString*)identifier
                              icon:(UIImage*)icon
                             title:(NSString*)title
           accessibilityIdentifier:(NSString*)accessibilityIdentifier
                              menu:(UIMenu*)menu {
  CHECK(icon);
  CHECK(menu);
  return [[self alloc] initWithStyle:IntelligenceHeaderItemStyle::kIcon
                          identifier:identifier
                                icon:icon
                               title:title
             accessibilityIdentifier:accessibilityIdentifier
                                menu:menu];
}

+ (instancetype)callToActionWithIdentifier:(NSString*)identifier
                                     title:(NSString*)title
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  return [[self alloc] initWithStyle:IntelligenceHeaderItemStyle::kCallToAction
                          identifier:identifier
                                icon:nil
                               title:title
             accessibilityIdentifier:accessibilityIdentifier
                                menu:nil];
}

#pragma mark - Private

- (instancetype)initWithStyle:(IntelligenceHeaderItemStyle)style
                   identifier:(NSString*)identifier
                         icon:(UIImage*)icon
                        title:(NSString*)title
      accessibilityIdentifier:(NSString*)accessibilityIdentifier
                         menu:(UIMenu*)menu {
  CHECK_GT(identifier.length, 0u);
  CHECK(title);

  self = [super init];
  if (self) {
    _style = style;
    _identifier = [identifier copy];
    _icon = icon;
    _title = [title copy];
    _accessibilityIdentifier = [accessibilityIdentifier copy];
    _menu = menu;
  }
  return self;
}

@end
