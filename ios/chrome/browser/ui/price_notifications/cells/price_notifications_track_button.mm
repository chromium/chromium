// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_track_button.h"

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_track_button_util.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_constants.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/sdk_forward_declares.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTrackButtonTopPadding = 4;
}  // namespace

@implementation PriceNotificationsTrackButton

- (instancetype)init {
  self = [super init];
  if (self) {
    // TODO(crbug.com/1418068): Simplify after minimum version required is >=
    // iOS 15.
    size_t horizontalPadding = [self horizontalPadding];
    if (base::ios::IsRunningOnIOS15OrLater() &&
        IsUIButtonConfigurationEnabled()) {
      if (@available(iOS 15, *)) {
        UIButtonConfiguration* buttonConfiguration =
            [UIButtonConfiguration plainButtonConfiguration];
        buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
            kTrackButtonTopPadding, horizontalPadding, kTrackButtonTopPadding,
            horizontalPadding);

        // Customize title string.
        NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
        UIFont* font =
            [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
        [style setLineBreakMode:NSLineBreakByTruncatingTail];
        NSDictionary* attributes = @{
          NSParagraphStyleAttributeName : style,
          NSFontAttributeName : font
        };
        NSMutableAttributedString* string = [[NSMutableAttributedString alloc]
            initWithString:
                l10n_util::GetNSString(
                    IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACK_BUTTON)];
        [string addAttributes:attributes range:NSMakeRange(0, string.length)];
        buttonConfiguration.attributedTitle = string;

        buttonConfiguration.baseForegroundColor = UIColor.whiteColor;
        buttonConfiguration.background.backgroundColor =
            [UIColor colorNamed:kBlueColor];
        buttonConfiguration.cornerStyle =
            UIButtonConfigurationCornerStyleCapsule;
        buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;
        self.configuration = buttonConfiguration;
      }
    } else {
      self.titleLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      [self.titleLabel setLineBreakMode:NSLineBreakByTruncatingTail];
      [self setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                 forState:UIControlStateNormal];
      self.backgroundColor = [UIColor colorNamed:kBlueColor];
      self.accessibilityIdentifier =
          kPriceNotificationsListItemTrackButtonIdentifier;
      [self setTitle:l10n_util::GetNSString(
                         IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACK_BUTTON)
            forState:UIControlStateNormal];
      UIEdgeInsets contentEdgeInsets =
          UIEdgeInsetsMake(kTrackButtonTopPadding, horizontalPadding,
                           kTrackButtonTopPadding, horizontalPadding);
      SetContentEdgeInsets(self, contentEdgeInsets);
    }
  }
  return self;
}

#pragma mark - Layout

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius = self.frame.size.height / 2;
  size_t horizontalPadding = [self horizontalPadding];

  price_notifications::WidthConstraintValues constraintValues =
      price_notifications::CalculateTrackButtonWidthConstraints(
          self.superview.superview.frame.size.width,
          self.titleLabel.intrinsicContentSize.width, horizontalPadding);
  [NSLayoutConstraint activateConstraints:@[
    [self.widthAnchor
        constraintLessThanOrEqualToConstant:constraintValues.max_width],
    [self.widthAnchor
        constraintGreaterThanOrEqualToConstant:constraintValues.target_width]
  ]];
}

#pragma mark - Private

// Returns the horizontal padding for contentInsets/contentEdgeInsets.
- (size_t)horizontalPadding {
  return price_notifications::CalculateTrackButtonHorizontalPadding(
      self.superview.superview.frame.size.width,
      self.titleLabel.intrinsicContentSize.width);
}

@end
