// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_view_controller.h"

#include "base/notreached.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface EnterprisePromptViewController ()

// PromptType that contains the type of the prompt to display.
@property(nonatomic, assign) EnterprisePromptType promptType;

@end

@implementation EnterprisePromptViewController

#pragma mark - Public

- (instancetype)initWithpromptType:(EnterprisePromptType)promptType {
  if (self = [super init]) {
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

  // TODO(crbug.com/1261423): Implement all cases.
  switch (self.promptType) {
    case EnterprisePromptTypeRestrictAccountSignedOut:
      [self restrictAccountSignedOut];
      break;
    case EnterprisePromptTypeForceSignOut:
    case EnterprisePromptTypeSyncDisabled:
      NOTREACHED();
      break;
  }

  if (@available(iOS 15, *)) {
    self.titleTextStyle = UIFontTextStyleTitle2;
    // Icon already contains some spacing for the shadow.
    self.customSpacingBeforeImageIfNoToolbar = 24;
    self.customSpacingAfterImage = 1;
    self.topAlignedLayout = YES;
  }

  [super loadView];
}

#pragma mark - Private

// Updates the view with account restriction informations.
- (void)restrictAccountSignedOut {
  self.titleString = l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT);
  self.subtitleString = l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_RESTRICTED_ACCOUNTS_TO_PATTERNS_MESSAGE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT_CONTINUE);
}

@end
