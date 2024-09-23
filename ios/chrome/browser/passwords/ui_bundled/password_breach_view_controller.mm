// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/password_breach_view_controller.h"

#import "ios/chrome/browser/passwords/ui_bundled/password_constants.h"
#import "ios/chrome/browser/shared/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation PasswordBreachViewController

#pragma mark - Public

- (void)loadView {
  self.helpButtonAvailable = YES;
  self.titleView = [self setUpTitleView];
  self.helpButtonAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HELP_ACCESSIBILITY_LABEL);

  self.image = [UIImage imageNamed:@"password_breach_illustration"];
  self.showDismissBarButton = NO;

  [super loadView];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kPasswordBreachViewAccessibilityIdentifier;
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self);
}

#pragma mark - PasswordBreachConsumer

- (void)setTitleString:(NSString*)titleString
           subtitleString:(NSString*)subtitleString
      primaryActionString:(NSString*)primaryActionString
    secondaryActionString:(NSString*)secondaryActionString {
  self.titleString = titleString;
  self.subtitleString = subtitleString;
  self.primaryActionString = primaryActionString;
  self.secondaryActionString = secondaryActionString;
}

#pragma mark - Private methods

// Configures the title view of this ViewController.
- (UIView*)setUpTitleView {
  NSString* title = l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);
  return password_manager::CreatePasswordManagerTitleView(title);
}

@end
