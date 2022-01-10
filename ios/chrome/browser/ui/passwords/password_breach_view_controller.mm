// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_view_controller.h"

#include "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/ui/passwords/password_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Picture width of the branding.
constexpr CGFloat kLogoWidth = 180;

}  // namespace

@implementation PasswordBreachViewController

#pragma mark - Public

- (void)loadView {
  self.helpButtonAvailable = YES;
  self.helpButtonAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HELP_ACCESSIBILITY_LABEL);

  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSEnablePasswordManagerBrandingUpdate)) {
    self.image = [UIImage imageNamed:@"password_breach_illustration"];
    self.showDismissBarButton = NO;
  } else {
    self.image = [UIImage imageNamed:@"legacy_password_breach_illustration"];
  }

  [super loadView];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kPasswordBreachViewAccessibilityIdentifier;

  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSEnablePasswordManagerBrandingUpdate)) {
    UIImageView* logoImageView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"passwords_logo_colored"]];
    logoImageView.contentMode = UIViewContentModeScaleAspectFit;
    logoImageView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.specificContentSuperview addSubview:logoImageView];

    logoImageView.isAccessibilityElement = YES;
    logoImageView.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_MANAGER_LOGO_ACCESSIBILITY_LABEL);

    [NSLayoutConstraint activateConstraints:@[
      [logoImageView.topAnchor
          constraintEqualToAnchor:self.specificContentLayoutGuide.topAnchor],
      [logoImageView.centerXAnchor
          constraintEqualToAnchor:self.specificContentLayoutGuide
                                      .centerXAnchor],
      [logoImageView.widthAnchor
          constraintLessThanOrEqualToConstant:kLogoWidth],
      [logoImageView.bottomAnchor
          constraintEqualToAnchor:self.specificContentLayoutGuide.bottomAnchor],
    ]];
  }
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

@end
