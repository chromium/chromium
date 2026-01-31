// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/coordinator/quick_delete_other_data_coordinator.h"

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation QuickDeleteOtherDataCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  return [super initWithBaseViewController:viewController browser:browser];
}

#pragma mark - ChromeCoordinator

- (void)start {
  // The "other data" page is only available on the regular browser.
  CHECK(!self.profile->IsOffTheRecord());

  // TODO(crbug.com/464551106): Implement the coordinator and create the
  // view controller and the necessary delegates.
}

@end
