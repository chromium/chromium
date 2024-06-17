// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_view_controller.h"

#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view.h"

@implementation DefaultBrowserGenericPromoViewController


#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view = [[DefaultBrowserInstructionsView alloc]
      initWithDismissButton:YES
           hasRemindMeLater:self.hasRemindMeLater
                   hasSteps:NO
              actionHandler:self.actionHandler];
}

@end
