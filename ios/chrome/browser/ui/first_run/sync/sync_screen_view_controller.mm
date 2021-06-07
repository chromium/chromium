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

namespace {
constexpr CGFloat kMarginBetweenContents = 12;
}  // namespace

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
  self.scrollToEndMandatory = YES;

  // Add sync screen-specific content
  UILabel* contentText = [self createContentText];
  [self.specificContentView addSubview:contentText];

  UIButton* advanceSyncSettingsButton = [self createAdvanceSyncSettingsButton];
  [self.specificContentView addSubview:advanceSyncSettingsButton];

  // Sync screen-specific constraints.
  [NSLayoutConstraint activateConstraints:@[
    [contentText.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [contentText.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [contentText.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [advanceSyncSettingsButton.topAnchor
        constraintEqualToAnchor:contentText.bottomAnchor
                       constant:kMarginBetweenContents],
    [advanceSyncSettingsButton.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [advanceSyncSettingsButton.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [advanceSyncSettingsButton.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];

  [super viewDidLoad];
}

#pragma mark - Private

// Creates and configures the text of sync screen
- (UILabel*)createContentText {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.numberOfLines = 0;
  label.textColor = [UIColor colorNamed:kGrey600Color];

  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;

  if (self.unifiedButtonStyle) {
    label.text = l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_SYNC_SCREEN_CONTENT_MINOR_MODE);
  } else {
    label.text = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SYNC_SCREEN_CONTENT);
  }
  return label;
}

// Creates and configures the sync settings button
- (UIButton*)createAdvanceSyncSettingsButton {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  [button.titleLabel
      setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]];
  [button setTitle:l10n_util::GetNSString(
                       IDS_IOS_FIRST_RUN_SYNC_SCREEN_ADVANCE_SETTINGS)
          forState:UIControlStateNormal];
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateNormal];

  [button addTarget:self
                action:@selector(showAdvanceSyncSettings)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Called when the sync settings button is tapped
- (void)showAdvanceSyncSettings {
  [self.delegate showSyncSettings];
}

@end
