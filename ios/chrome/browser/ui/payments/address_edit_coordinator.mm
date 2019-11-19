// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_coordinator.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/payments/address_edit_mediator.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/payments/payment_request_navigation_controller.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::AutofillUITypeFromAutofillType;
using ::AutofillTypeFromAutofillUIType;
}  // namespace

@interface AddressEditCoordinator ()

@property(nonatomic, strong)
    CountrySelectionCoordinator* countrySelectionCoordinator;

@property(nonatomic, strong) PaymentRequestNavigationController* viewController;

@property(nonatomic, strong)
    PaymentRequestEditViewController* editViewController;

@property(nonatomic, strong) AddressEditMediator* mediator;

@end

@implementation AddressEditCoordinator

@synthesize address = _address;
@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize countrySelectionCoordinator = _countrySelectionCoordinator;
@synthesize viewController = _viewController;
@synthesize editViewController = _editViewController;
@synthesize mediator = _mediator;

- (void)start {
  self.editViewController = [[PaymentRequestEditViewController alloc] init];
  [self.editViewController setDelegate:self];
  self.mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:self.paymentRequest
                                                  address:self.address];
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
  [self.countrySelectionCoordinator stop];
  self.countrySelectionCoordinator = nil;
  self.editViewController = nil;
  self.viewController = nil;
}

#pragma mark - PaymentRequestEditViewControllerDelegate

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                          didSelectField:(EditorField*)field {
  if (field.autofillUIType == AutofillUITypeProfileHomeAddressCountry) {
    self.countrySelectionCoordinator = [[CountrySelectionCoordinator alloc]
        initWithBaseViewController:self.editViewController];
    [self.countrySelectionCoordinator setCountries:self.mediator.countries];
    [self.countrySelectionCoordinator
        setSelectedCountryCode:self.mediator.selectedCountryCode];
    [self.countrySelectionCoordinator setDelegate:self];
    [self.countrySelectionCoordinator start];
  }
}

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                  didFinishEditingFields:(NSArray<EditorField*>*)fields {
  // Create an empty autofill profile. If an address is being edited, copy over
  // the information.
  autofill::AutofillProfile address =
      self.address ? *self.address : autofill::AutofillProfile();

  // Set the origin, or override it if the autofill profile is being edited.
  address.set_origin(autofill::kSettingsOrigin);

  for (EditorField* field in fields) {
    address.SetInfo(autofill::AutofillType(
                        AutofillTypeFromAutofillUIType(field.autofillUIType)),
                    base::SysNSStringToUTF16(field.value),
                    self.paymentRequest->GetApplicationLocale());
  }

  if (!self.address) {
    // Add the profile to the list of profiles in |self.paymentRequest|.
    self.address = self.paymentRequest->AddAutofillProfile(address);
  } else {
    // Update the original profile instance that is being edited.
    *self.address = address;
    self.paymentRequest->UpdateAutofillProfile(address);
  }

  [self.delegate addressEditCoordinator:self
                didFinishEditingAddress:self.address];
}

- (void)paymentRequestEditViewControllerDidCancel:
    (PaymentRequestEditViewController*)controller {
  [self.delegate addressEditCoordinatorDidCancel:self];
}

#pragma mark - CountrySelectionCoordinatorDelegate

- (void)countrySelectionCoordinator:(CountrySelectionCoordinator*)coordinator
           didSelectCountryWithCode:(NSString*)countryCode {
  if (self.mediator.selectedCountryCode != countryCode) {
    [self.mediator setSelectedCountryCode:countryCode];
    [self.editViewController loadModel];
    [self.editViewController.collectionView reloadData];
  }
  [self.countrySelectionCoordinator stop];
  self.countrySelectionCoordinator = nil;
}

- (void)countrySelectionCoordinatorDidReturn:
    (CountrySelectionCoordinator*)coordinator {
  [self.countrySelectionCoordinator stop];
  self.countrySelectionCoordinator = nil;
}

@end
