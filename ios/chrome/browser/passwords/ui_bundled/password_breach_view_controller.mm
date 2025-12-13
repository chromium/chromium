// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/password_breach_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kHelpSymbolSize = 20;
}  // namespace

@implementation PasswordBreachViewController {
  UIBarButtonItem* _helpButton;
  // Popover used to show learn more info, not nil when presented.
  PopoverLabelViewController* _learnMoreViewController;
}

#pragma mark - Public

- (void)viewDidLoad {
  _helpButton = [[UIBarButtonItem alloc]
      initWithImage:DefaultSymbolWithPointSize(kHelpSymbol, kHelpSymbolSize)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(showLearnMore)];

  _helpButton.isAccessibilityElement = YES;
  _helpButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HELP_ACCESSIBILITY_LABEL);
  self.navigationItem.leftBarButtonItem = _helpButton;

  NSString* title = l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);
  self.navigationItem.titleView =
      password_manager::CreatePasswordManagerTitleView(title);

  self.image = [UIImage imageNamed:@"password_breach_illustration"];

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
  self.configuration.primaryActionString = primaryActionString;
  self.configuration.secondaryActionString = secondaryActionString;
  [self reloadConfiguration];
}

#pragma mark - Private methods

// Presents more information related to the feature.
- (void)showLearnMore {
  NSString* message =
      l10n_util::GetNSString(IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE);
  _learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];
  [self presentViewController:_learnMoreViewController
                     animated:YES
                   completion:nil];
  _learnMoreViewController.popoverPresentationController.barButtonItem =
      _helpButton;
  _learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;
}

@end
