// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SyncScreenViewController

@dynamic delegate;

- (void)viewDidLoad {
  self.titleText = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SYNC_SCREEN_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SYNC_SCREEN_SUBTITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SYNC_SCREEN_PRIMARY_ACTION);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SYNC_SCREEN_SECONDARY_ACTION);

  self.bannerImage = [UIImage imageNamed:@"sync_screen_banner"];
  self.isTallBanner = NO;

  // Add sync screen-specific content and its constraints.
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.numberOfLines = 0;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;

  if (self.unifiedButtonStyle) {
    label.text = l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_SYNC_SCREEN_CONTENT_MINOR_MODE);
  } else {
    label.text = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SYNC_SCREEN_CONTENT);
  }

  [self.specificContentView addSubview:label];

  [NSLayoutConstraint activateConstraints:@[
    [label.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                 .topAnchor],
    [label.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [label.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
  ]];

  [super viewDidLoad];
}

@end
