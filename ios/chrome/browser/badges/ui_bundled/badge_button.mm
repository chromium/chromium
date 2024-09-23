// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/badge_button.h"

#import <ostream>

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

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
  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateDefaultEffectCirclePointerStyleProvider();
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
    self.accessibilityIdentifier =
        [self accessibilityIdentifierForAcceptedState:accepted];
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

- (NSString*)accessibilityIdentifierForAcceptedState:(BOOL)accepted {
  switch (self.badgeType) {
    case kBadgeTypeNone:
      NOTREACHED_IN_MIGRATION() << "A badge should not have kBadgeTypeNone";
      return nil;
    case kBadgeTypePasswordSave:
      return accepted ? kBadgeButtonSavePasswordAcceptedAccessibilityIdentifier
                      : kBadgeButtonSavePasswordAccessibilityIdentifier;
    case kBadgeTypePasswordUpdate:
      return accepted
                 ? kBadgeButtonUpdatePasswordAccpetedAccessibilityIdentifier
                 : kBadgeButtonUpdatePasswordAccessibilityIdentifier;
    case kBadgeTypeIncognito:
      return kBadgeButtonIncognitoAccessibilityIdentifier;
    case kBadgeTypeOverflow:
      return kBadgeButtonOverflowAccessibilityIdentifier;
    case kBadgeTypeSaveCard:
      return accepted ? kBadgeButtonSaveCardAcceptedAccessibilityIdentifier
                      : kBadgeButtonSaveCardAccessibilityIdentifier;
    case kBadgeTypeSaveAddressProfile:
      return accepted
                 ? kBadgeButtonSaveAddressProfileAcceptedAccessibilityIdentifier
                 : kBadgeButtonSaveAddressProfileAccessibilityIdentifier;
    case kBadgeTypeTranslate:
      return accepted ? kBadgeButtonTranslateAcceptedAccessibilityIdentifier
                      : kBadgeButtonTranslateAccessibilityIdentifier;
    case kBadgeTypePermissionsCamera:
      return accepted
                 ? kBadgeButtonPermissionsCameraAcceptedAccessibilityIdentifier
                 : kBadgeButtonPermissionsCameraAccessibilityIdentifier;
    case kBadgeTypePermissionsMicrophone:
      return accepted
                 ? kBadgeButtonPermissionsMicrophoneAcceptedAccessibilityIdentifier
                 : kBadgeButtonPermissionsMicrophoneAccessibilityIdentifier;
    case kBadgeTypeParcelTracking:
      return accepted
                 ? kBadgeButtonParcelTrackingAcceptedAccessibilityIdentifier
                 : kBadgeButtonParcelTrackingAccessibilityIdentifier;
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
