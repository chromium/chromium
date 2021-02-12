// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_navigation_controller.h"

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

@implementation AdvancedSettingsSigninNavigationController

#pragma mark - UINavigationController

- (void)viewDidLoad {
  [super viewDidLoad];

  if (base::FeatureList::IsEnabled(kSettingsRefresh)) {
    self.navigationBar.barTintColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];
    self.toolbar.barTintColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  }
}

- (void)pushViewController:(UIViewController*)viewController
                  animated:(BOOL)animated {
  [super pushViewController:viewController animated:animated];
  if (self.viewControllers.count == 1) {
    viewController.navigationItem.leftBarButtonItem =
        [self navigationCancelButton];
    viewController.navigationItem.rightBarButtonItem =
        [self navigationConfirmButton];
  }
}

#pragma mark - Private

// Creates a cancel button for the navigation item.
- (UIBarButtonItem*)navigationCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(onNavigationCancelButton)];
  cancelButton.accessibilityIdentifier = kSyncSettingsCancelButtonId;
  return cancelButton;
}

// Creates a confirmation button for the navigation item.
- (UIBarButtonItem*)navigationConfirmButton {
  UIBarButtonItem* confirmButton = [[UIBarButtonItem alloc]
      initWithTitle:GetNSString(
                        IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CONFIRM_MAIN_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(onNavigationConfirmButton)];
  confirmButton.accessibilityIdentifier = kSyncSettingsConfirmButtonId;
  return confirmButton;
}

#pragma mark - Button events

// Called by the cancel button from the navigation controller. Shows the cancel
// alert dialog.
- (void)onNavigationCancelButton {
  [self.navigationDelegate navigationCancelButtonWasTapped];
}

// Called by the confirm button from tne navigation controller. Validates the
// sync preferences chosen by the user, starts the sync, close the completion
// callback and closes the advanced sign-in settings.
- (void)onNavigationConfirmButton {
  [self.navigationDelegate navigationConfirmButtonWasTapped];
}

@end
