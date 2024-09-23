// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_view_controller.h"

#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface EnterprisePromptViewController ()

// PromptType that contains the type of the prompt to display.
@property(nonatomic, assign) EnterprisePromptType promptType;

@end

@implementation EnterprisePromptViewController

#pragma mark - Public

- (instancetype)initWithpromptType:(EnterprisePromptType)promptType {
  if ((self = [super init])) {
    _promptType = promptType;
  }
  return self;
}

- (void)loadView {
  self.image = [UIImage imageNamed:@"enterprise_grey_icon_large"];
  self.imageHasFixedSize = YES;
  self.customSpacingAfterImage = 30;

  self.showDismissBarButton = NO;

  self.dismissBarButtonSystemItem = UIBarButtonSystemItemDone;

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT_CONTINUE);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT_LEARN_MORE);

  switch (self.promptType) {
    case EnterprisePromptTypeRestrictAccountSignedOut:
      [self setupForRestrictAccountSignedOut];
      break;
    case EnterprisePromptTypeForceSignOut:
      [self setupForForceSignOut];
      break;
    case EnterprisePromptTypeSyncDisabled:
      [self setupForSyncDisabled];
      break;
  }

  self.titleTextStyle = UIFontTextStyleTitle2;
  // Icon already contains some spacing for the shadow.
  self.customSpacingBeforeImageIfNoNavigationBar = 24;
  self.customSpacingAfterImage = 1;
  self.topAlignedLayout = YES;

  [super loadView];
}

#pragma mark - Private

// Updates the view with account restriction informations.
- (void)setupForRestrictAccountSignedOut {
  self.titleString = l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT);
  self.subtitleString = l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_RESTRICTED_ACCOUNTS_TO_PATTERNS_MESSAGE);
}

// Updates the view with force sign out informations.
- (void)setupForForceSignOut {
  self.titleString = l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT_MESSAGE_WITH_UNO);
}

// Updates the view with sync disabled informations.
- (void)setupForSyncDisabled {
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SYNC_DISABLED_TITLE_WITH_UNO);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SYNC_DISABLED_MESSAGE_WITH_UNO);
}

@end
