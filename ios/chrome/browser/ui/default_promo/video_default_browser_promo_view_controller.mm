// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_view_controller.h"

#import "ios/chrome/browser/ui/default_promo/default_browser_instructions_view.h"

@implementation VideoDefaultBrowserPromoViewController


#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view = [[DefaultBrowserInstructionsView alloc]
      initWithDismissButton:YES
           hasRemindMeLater:self.showRemindMeLater
                   hasSteps:NO
              actionHandler:self.actionHandler];
}

@end
