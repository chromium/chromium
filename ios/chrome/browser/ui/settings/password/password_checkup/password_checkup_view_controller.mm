// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordCheckupViewController () {
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}
@end

@implementation PasswordCheckupViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.navigationController.navigationBar.backgroundColor =
      [UIColor colorNamed:@"password_checkup_header_background_color"];
}

- (void)willMoveToParentViewController:(UIViewController*)parent {
  [super willMoveToParentViewController:parent];
  // Set the navigation bar background color back to `nil`.
  super.navigationController.navigationBar.backgroundColor = nil;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.handler dismissPasswordCheckupViewController];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordCheckupSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordCheckupSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  _settingsAreDismissed = YES;
}

#pragma mark - PasswordCheckupConsumer

- (void)setPasswordCheckupHomepageState:(PasswordCheckupHomepageState)state
                 insecurePasswordCounts:
                     (password_manager::InsecurePasswordCounts)
                         insecurePasswordCounts {
  // TODO(crbug.com/1406540): Add method's body.
}

- (void)setAffiliatedGroupCount:(NSInteger)affiliatedGroupCount {
  // TODO(crbug.com/1406540): Add method's body.
}

@end
