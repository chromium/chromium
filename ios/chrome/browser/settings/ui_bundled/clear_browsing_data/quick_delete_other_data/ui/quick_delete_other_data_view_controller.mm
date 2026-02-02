// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/ui/quick_delete_other_data_view_controller.h"

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/public/quick_delete_other_data_commands.h"

@implementation QuickDeleteOtherDataViewController

// TODO(crbug.com/464551506): Add the implementation for the view controller.
- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.quickDeleteOtherDataHandler hideQuickDeleteOtherDataPage];
  }
}

@end
