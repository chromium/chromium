// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/auto_deletion/auto_deletion_iph_coordinator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/download/ui_bundled/auto_deletion/auto_deletion_iph_view_controller.h"
#import "ios/web/public/download/download_task.h"
#import "ui/base/device_form_factor.h"

@implementation AutoDeletionIPHCoordinator {
  // The ViewController for the Auto-deletion IPH.
  AutoDeletionIPHViewController* _viewController;
}

- (void)start {
  _viewController = [[AutoDeletionIPHViewController alloc] init];

  UISheetPresentationController* sheetPresentationController =
      _viewController.sheetPresentationController;
  if (sheetPresentationController) {
    sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
    sheetPresentationController
        .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

    sheetPresentationController.detents =
        ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
            ? @[ [UISheetPresentationControllerDetent largeDetent] ]
            : @[
                [UISheetPresentationControllerDetent mediumDetent],
                [UISheetPresentationControllerDetent largeDetent]
              ];
  }

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nullptr;
}

@end
