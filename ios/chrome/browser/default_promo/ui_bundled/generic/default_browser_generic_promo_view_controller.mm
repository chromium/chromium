// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_view_controller.h"

#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation DefaultBrowserGenericPromoViewController {
  NSString* _titleText;
}

#pragma mark - UIViewController

- (void)loadView {
  ConfirmationAlertViewController* alertScreenViewController =
      [[ConfirmationAlertViewController alloc] init];
  [self addChildViewController:alertScreenViewController];

  if (IsSegmentedDefaultBrowserPromoEnabled()) {
    self.view = [[DefaultBrowserInstructionsView alloc]
            initWithDismissButton:YES
                 hasRemindMeLater:self.hasRemindMeLater
                         hasSteps:NO
                    actionHandler:self.actionHandler
        alertScreenViewController:alertScreenViewController
                        titleText:_titleText];
  } else {
    self.view = [[DefaultBrowserInstructionsView alloc]
            initWithDismissButton:YES
                 hasRemindMeLater:self.hasRemindMeLater
                         hasSteps:NO
                    actionHandler:self.actionHandler
        alertScreenViewController:alertScreenViewController
                        titleText:nil];
  }
}

#pragma mark - DefaultBrowserGenericPromoConsumer

- (void)setPromoTitle:(NSString*)titleText {
  _titleText = titleText;
}

@end
