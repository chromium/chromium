// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/contact_info_edit_coordinator.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/payments/contact_info_edit_mediator.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/payments/payment_request_navigation_controller.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::AutofillTypeFromAutofillUIType;
}  // namespace

@interface ContactInfoEditCoordinator ()

@property(nonatomic, strong) PaymentRequestNavigationController* viewController;

@property(nonatomic, strong)
    PaymentRequestEditViewController* editViewController;

@property(nonatomic, strong) ContactInfoEditMediator* mediator;

@end

@implementation ContactInfoEditCoordinator

@synthesize profile = _profile;
@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize viewController = _viewController;
@synthesize editViewController = _editViewController;
@synthesize mediator = _mediator;

- (void)start {
  self.editViewController = [[PaymentRequestEditViewController alloc] init];
  [self.editViewController setDelegate:self];
  self.mediator = [[ContactInfoEditMediator alloc]
      initWithPaymentRequest:self.paymentRequest
                     profile:self.profile];
  [self.mediator setConsumer:self.editViewController];
  [self.editViewController setDataSource:self.mediator];
  [self.editViewController setValidatorDelegate:self.mediator];
  [self.editViewController loadModel];

  self.viewController = [[PaymentRequestNavigationController alloc]
      initWithRootViewController:self.editViewController];
  self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  self.viewController.modalTransitionStyle =
      UIModalTransitionStyleCoverVertical;
  self.viewController.navigationBarHidden = YES;

  [[self baseViewController] presentViewController:self.viewController
                                          animated:YES
                                        completion:nil];
}

- (void)stop {
  [[self.viewController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.editViewController = nil;
  self.viewController = nil;
}

#pragma mark - PaymentRequestEditViewControllerDelegate

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                  didFinishEditingFields:(NSArray<EditorField*>*)fields {
  // Create an empty autofill profile. If a profile is being edited, copy over
  // the information.
  autofill::AutofillProfile profile =
      self.profile ? *self.profile : autofill::AutofillProfile();
  // Set the origin, or override it if the autofill profile is being edited.
  profile.set_origin(autofill::kSettingsOrigin);

  for (EditorField* field in fields) {
    profile.SetInfo(autofill::AutofillType(
                        AutofillTypeFromAutofillUIType(field.autofillUIType)),
                    base::SysNSStringToUTF16(field.value),
                    self.paymentRequest->GetApplicationLocale());
  }

  if (!self.profile) {
    // Add the profile to the list of profiles in |self.paymentRequest|.
    self.profile = self.paymentRequest->AddAutofillProfile(profile);
  } else {
    // Update the original profile instance that is being edited.
    *self.profile = profile;
    self.paymentRequest->UpdateAutofillProfile(profile);
  }

  [self.delegate contactInfoEditCoordinator:self
                    didFinishEditingProfile:self.profile];
}

- (void)paymentRequestEditViewControllerDidCancel:
    (PaymentRequestEditViewController*)controller {
  [self.delegate contactInfoEditCoordinatorDidCancel:self];
}

@end
