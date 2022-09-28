// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_view_controller.h"

#import "ios/chrome/browser/ui/elements/instruction_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// URL for the Settings link.
const char* const kSettingsSyncURL = "internal://settings-sync";

}  // namespace

@implementation TangibleSyncViewController

@synthesize primaryIdentityAvatarImage = _primaryIdentityAvatarImage;

- (void)viewDidLoad {
  self.shouldHideBanner = YES;
  self.hasAvatarImage = YES;
  self.avatarImage = self.primaryIdentityAvatarImage;
  self.titleText = l10n_util::GetNSString(IDS_IOS_TANGIBLE_SYNC_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_TANGIBLE_SYNC_SUBTITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON);
  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION);
  self.disclaimerText =
      l10n_util::GetNSString(IDS_IOS_TANGIBLE_SYNC_DISCLAIMER);
  self.disclaimerURLs = @[ net::NSURLWithGURL(GURL(kSettingsSyncURL)) ];
  NSArray<NSString*>* dataTypeNames = @[
    l10n_util::GetNSString(IDS_IOS_TANGIBLE_SYNC_DATA_TYPE_BOOKMARKS),
    l10n_util::GetNSString(IDS_IOS_TANGIBLE_SYNC_DATA_TYPE_AUTOFILL),
    l10n_util::GetNSString(IDS_IOS_TANGIBLE_SYNC_DATA_TYPE_HISTORY),
  ];
  // TODO(crbug.com/1363812): Need to icon version with custom symbols.
  NSArray<UIImage*>* dataTypeIcons = @[
    [UIImage imageNamed:@"tangible_sync_bookmarks"],
    [UIImage imageNamed:@"tangible_sync_autofill"],
    [UIImage imageNamed:@"tangible_sync_history"],
  ];
  InstructionView* instructionView =
      [[InstructionView alloc] initWithList:dataTypeNames
                                      style:InstructionViewStyleDefault
                                      icons:dataTypeIcons];
  instructionView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.specificContentView addSubview:instructionView];
  [NSLayoutConstraint activateConstraints:@[
    [instructionView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [instructionView.leadingAnchor
        constraintEqualToAnchor:self.specificContentView.leadingAnchor],
    [instructionView.trailingAnchor
        constraintEqualToAnchor:self.specificContentView.trailingAnchor],
  ]];
  [super viewDidLoad];
}

#pragma mark - TangibleSyncConsumer

- (void)setPrimaryIdentityAvatarImage:(UIImage*)primaryIdentityAvatarImage {
  if (_primaryIdentityAvatarImage != primaryIdentityAvatarImage) {
    _primaryIdentityAvatarImage = primaryIdentityAvatarImage;
    self.avatarImage = primaryIdentityAvatarImage;
  }
}

@end
