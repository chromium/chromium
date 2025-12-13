// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_cvc_storage_view_coordinator.h"

#import "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_cvc_storage_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_cvc_storage_view_mediator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation AutofillCvcStorageViewCoordinator {
  AutofillCvcStorageViewController* _viewController;
  AutofillCvcStorageViewMediator* _mediator;
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
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          self.browser->GetProfile());
  _mediator = [[AutofillCvcStorageViewMediator alloc]
      initWithPersonalDataManager:personalDataManager
                      prefService:self.browser->GetProfile()->GetPrefs()];
  _viewController.delegate = _mediator;
  _mediator.consumer = _viewController;

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  [super stop];
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

@end
