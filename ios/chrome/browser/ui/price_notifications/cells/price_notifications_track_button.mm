// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_track_button.h"

#import "ios/chrome/browser/ui/price_notifications/price_notifications_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTrackButtonSidePadding = 16;
}  // namespace

@implementation PriceNotificationsTrackButton

- (instancetype)init {
  self = [super init];
  if (self) {
    self.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    self.tintColor = [UIColor colorNamed:kSolidButtonTextColor];
    self.backgroundColor = [UIColor colorNamed:kBlueColor];
    self.accessibilityIdentifier =
        kPriceNotificationsListItemTrackButtonIdentifier;
    [self setTitle:l10n_util::GetNSString(
                       IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACK_BUTTON)
          forState:UIControlStateNormal];

    self.contentEdgeInsets = UIEdgeInsetsMake(0, kTrackButtonSidePadding, 0,
                                              kTrackButtonSidePadding);
  }
  return self;
}

#pragma mark - Layout

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius = self.frame.size.height / 2;
}

@end
