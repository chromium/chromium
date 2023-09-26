// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_view_controller.h"

#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation WidgetPromoInstructionsViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.view.accessibilityIdentifier =
      password_manager::kWidgetPromoInstructionsViewID;
}

@end
