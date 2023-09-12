// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_view_controller.h"

#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
CGFloat const kSpacingAfterImage = 0;
NSString* const kOptInIcon = @"parcel_tracking_icon_new";
}  // namespace

@implementation ParcelTrackingOptInViewController

- (void)viewDidLoad {
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_TITLE);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_SUBTITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_PRIMARY_ACTION);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_SECONDARY_ACTION);
  self.tertiaryActionString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_TERTIARY_ACTION);
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.subtitleTextStyle = UIFontTextStyleCallout;
  self.showDismissBarButton = NO;
  self.image = [UIImage imageNamed:kOptInIcon];
  self.imageHasFixedSize = true;
  self.customSpacingAfterImage = kSpacingAfterImage;
  self.sheetPresentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  [super viewDidLoad];
}

@end
