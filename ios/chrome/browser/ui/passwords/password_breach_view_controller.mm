// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_view_controller.h"

#import "ios/chrome/browser/ui/passwords/password_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordBreachViewController

#pragma mark - Public

- (void)loadView {
  self.image = [UIImage imageNamed:@"password_breach_illustration"];
  self.helpButtonAvailable = YES;
  self.helpButtonAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HELP_ACCESSIBILITY_LABEL);

  if (@available(iOS 13.4, *)) {
      self.pointerInteractionEnabled = YES;
  }

  [super loadView];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kPasswordBreachViewAccessibilityIdentifier;
}

#pragma mark - PasswordBreachConsumer

- (void)setTitleString:(NSString*)titleString
            subtitleString:(NSString*)subtitleString
       primaryActionString:(NSString*)primaryActionString
    primaryActionAvailable:(BOOL)primaryActionAvailable {
  self.titleString = titleString;
  self.subtitleString = subtitleString;
  self.primaryActionString = primaryActionString;
  self.primaryActionAvailable = primaryActionAvailable;
}

@end
