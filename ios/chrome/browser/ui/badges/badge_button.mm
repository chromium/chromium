// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_button.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/badges/badge_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Duration of button animations, in seconds.
const CGFloat kButtonAnimationDuration = 0.2;
// To achieve a circular corner radius, divide length of a side by 2.
const CGFloat kButtonCircularCornerRadiusDivisor = 2.0;
}  // namespace

@interface BadgeButton ()

// Read/Write override.
@property(nonatomic, assign, readwrite) BadgeType badgeType;
// Read/Write override.
@property(nonatomic, assign, readwrite) BOOL accepted;

@end

@implementation BadgeButton

+ (instancetype)badgeButtonWithType:(BadgeType)badgeType {
  BadgeButton* button = [self buttonWithType:UIButtonTypeSystem];
  button.badgeType = badgeType;
  return button;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius =
      self.bounds.size.height / kButtonCircularCornerRadiusDivisor;
}

- (void)setAccepted:(BOOL)accepted animated:(BOOL)animated {
  self.accepted = accepted;
  void (^changeTintColor)() = ^{
    self.tintColor = accepted ? nil : [UIColor colorNamed:kToolbarButtonColor];
    self.accessibilityIdentifier = [self getAccessibilityIdentifier:accepted];
  };
  if (animated) {
    [UIView animateWithDuration:kButtonAnimationDuration
                     animations:changeTintColor];
  } else {
    changeTintColor();
  }
}

- (void)setFullScreenOn:(BOOL)fullScreenOn {
  if (_fullScreenOn == fullScreenOn) {
    return;
  }
  _fullScreenOn = fullScreenOn;
  [self configureImage];
}

#pragma mark - Setters

- (void)setImage:(UIImage*)image {
  _image = image;
  if (!self.fullScreenOn) {
    [self configureImage];
  }
}

#pragma mark - Private

- (NSString*)getAccessibilityIdentifier:(BOOL)accepted {
  switch (self.badgeType) {
    case BadgeType::kBadgeTypeNone:
      NOTREACHED() << "A badge should not have kBadgeTypeNone";
      return nil;
    case BadgeType::kBadgeTypePasswordSave:
      return accepted ? kBadgeButtonSavePasswordAcceptedAccessibilityIdentifier
                      : kBadgeButtonSavePasswordAccessibilityIdentifier;
    case BadgeType::kBadgeTypePasswordUpdate:
      return accepted
                 ? kBadgeButtonUpdatePasswordAccpetedAccessibilityIdentifier
                 : kBadgeButtonUpdatePasswordAccessibilityIdentifier;
    case BadgeType::kBadgeTypeIncognito:
      return kBadgeButtonIncognitoAccessibilityIdentifier;
    case BadgeType::kBadgeTypeOverflow:
      return kBadgeButtonOverflowAccessibilityIdentifier;
    case BadgeType::kBadgeTypeSaveCard:
      return accepted ? kBadgeButtonSaveCardAcceptedAccessibilityIdentifier
                      : kBadgeButtonSaveCardAccessibilityIdentifier;
    case BadgeType::kBadgeTypeTranslate:
      return accepted ? kBadgeButtonTranslateAcceptedAccessibilityIdentifier
                      : kBadgeButtonTranslateAccessibilityIdentifier;
  }
}

- (void)configureImage {
  if (self.fullScreenOn && self.fullScreenImage) {
    [self setImage:self.fullScreenImage forState:UIControlStateNormal];
    [self setImage:self.fullScreenImage forState:UIControlStateDisabled];
  } else {
    [self setImage:self.image forState:UIControlStateNormal];
    [self setImage:self.image forState:UIControlStateDisabled];
  }
}

@end
