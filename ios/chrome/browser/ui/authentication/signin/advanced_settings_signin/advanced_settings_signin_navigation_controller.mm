// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_navigation_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using l10n_util::GetNSString;

@implementation AdvancedSettingsSigninNavigationController

#pragma mark - UINavigationController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.navigationBar.barTintColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  self.toolbar.barTintColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
}

- (void)pushViewController:(UIViewController*)viewController
                  animated:(BOOL)animated {
  [super pushViewController:viewController animated:animated];
  if (self.viewControllers.count == 1) {
    viewController.navigationItem.rightBarButtonItem =
        [self navigationDoneButton];
  }
}

#pragma mark - Private

// Creates a confirmation button for the navigation item.
- (UIBarButtonItem*)navigationDoneButton {
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithTitle:GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(onNavigationDoneButton)];
  doneButton.accessibilityIdentifier = kAdvancedSyncSettingsDoneButtonMatcherId;
  return doneButton;
}

#pragma mark - Button events

// Called by the done button from the navigation controller. Validates the
// sync preferences chosen by the user, closes the completion
// callback and closes the advanced sign-in settings.
- (void)onNavigationDoneButton {
  [self.navigationDelegate navigationDoneButtonWasTapped:self];
}

@end
