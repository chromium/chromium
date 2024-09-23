// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/user_policy/user_policy_prompt_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kCustomSpacingBeforeImageIfNoNavigationBar = 24;
constexpr CGFloat kCustomSpacingAfterImage = 1;
}  // namespace

@implementation UserPolicyPromptViewController

- (instancetype)initWithManagedDomain:(NSString*)managedDomain {
  if ((self = [super init])) {
    self.titleString = l10n_util::GetNSString(
        IDS_IOS_USER_POLICY_NOTIFICATION_NO_SIGNOUT_TITLE);
    self.subtitleString = l10n_util::GetNSStringF(
        IDS_IOS_USER_POLICY_NOTIFICATION_NO_SIGNOUT_SUBTITLE,
        base::SysNSStringToUTF16(managedDomain));
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT_CONTINUE);
    self.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT_LEARN_MORE);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.image = [UIImage imageNamed:@"enterprise_grey_icon_large"];
  self.imageHasFixedSize = YES;
  self.showDismissBarButton = NO;
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemDone;

  self.titleTextStyle = UIFontTextStyleTitle2;
  // Icon already contains some spacing for the shadow.
  self.customSpacingBeforeImageIfNoNavigationBar =
      kCustomSpacingBeforeImageIfNoNavigationBar;
  self.customSpacingAfterImage = kCustomSpacingAfterImage;
  self.topAlignedLayout = YES;

  [super viewDidLoad];
}

@end
