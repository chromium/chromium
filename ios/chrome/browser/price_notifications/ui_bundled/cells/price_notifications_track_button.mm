// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_notifications/ui_bundled/cells/price_notifications_track_button.h"

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/price_notifications/ui_bundled/cells/price_notifications_track_button_util.h"
#import "ios/chrome/browser/price_notifications/ui_bundled/price_notifications_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
const CGFloat kTrackButtonTopPadding = 4;
const CGFloat kTrackButtonTopBottomPaddingLightVariant = 3;
const CGFloat kTrackButtonSidePaddingLightVariant = 14;
const CGFloat kTrackButtonCornerRadiusLightVariant = 76;
const CGFloat kTrackButtonLightVariantAlpha = 0.11f;
}  // namespace

@implementation PriceNotificationsTrackButton

- (instancetype)initWithLightVariant:(BOOL)useLightVariant {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _useLightVariant = useLightVariant;
    size_t horizontalPadding = [self horizontalPadding];
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    if (_useLightVariant) {
      buttonConfiguration.contentInsets =
          NSDirectionalEdgeInsetsMake(kTrackButtonTopBottomPaddingLightVariant,
                                      kTrackButtonSidePaddingLightVariant,
                                      kTrackButtonTopBottomPaddingLightVariant,
                                      kTrackButtonSidePaddingLightVariant);

    } else {
      buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
          kTrackButtonTopPadding, horizontalPadding, kTrackButtonTopPadding,
          horizontalPadding);
    }

    // Customize title string.
    UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSMutableAttributedString* string = [[NSMutableAttributedString alloc]
        initWithString:
            l10n_util::GetNSString(
                IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACK_BUTTON)];
    [string addAttributes:attributes range:NSMakeRange(0, string.length)];
    buttonConfiguration.attributedTitle = string;

    buttonConfiguration.baseForegroundColor = [UIColor
        colorNamed:(_useLightVariant) ? kBlue600Color : kSolidWhiteColor];

    if (_useLightVariant) {
      buttonConfiguration.background.backgroundColor =
          [[UIColor colorNamed:kBlue600Color]
              colorWithAlphaComponent:kTrackButtonLightVariantAlpha];
    } else {
      buttonConfiguration.background.backgroundColor =
          [UIColor colorNamed:kBlueColor];
    }

    buttonConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;
    self.configuration = buttonConfiguration;
  }
  return self;
}

#pragma mark - Layout

- (void)layoutSubviews {
  [super layoutSubviews];
  if (_useLightVariant) {
    self.layer.cornerRadius = kTrackButtonCornerRadiusLightVariant;
  } else {
    self.layer.cornerRadius = self.frame.size.height / 2;
  }
  size_t horizontalPadding = [self horizontalPadding];

  price_notifications::WidthConstraintValues constraintValues =
      price_notifications::CalculateTrackButtonWidthConstraints(
          self.superview.superview.frame.size.width,
          self.titleLabel.intrinsicContentSize.width,
          (_useLightVariant) ? kTrackButtonSidePaddingLightVariant
                             : horizontalPadding);

  if (_useLightVariant) {
    float light_button_width = self.titleLabel.intrinsicContentSize.width +
                               kTrackButtonSidePaddingLightVariant * 2;

    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintEqualToConstant:light_button_width]
    ]];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor
          constraintLessThanOrEqualToConstant:constraintValues.max_width],
      [self.widthAnchor
          constraintGreaterThanOrEqualToConstant:constraintValues.target_width]
    ]];
  }
}

#pragma mark - Private

// Returns the horizontal padding for contentInsets/contentEdgeInsets.
- (size_t)horizontalPadding {
  return price_notifications::CalculateTrackButtonHorizontalPadding(
      self.superview.superview.frame.size.width,
      self.titleLabel.intrinsicContentSize.width);
}

@end
