// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_cvc_storage_view_coordinator.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_cvc_storage_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_cvc_storage_view_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation AutofillCvcStorageViewCoordinator {
  AutofillCvcStorageViewController* _viewController;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:
                    (UINavigationController*)navigationController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  _baseNavigationController = navigationController;
  return self;
}

- (void)start {
  _viewController = [[AutofillCvcStorageViewController alloc] init];

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  [super stop];
  _viewController = nil;
}

@end
