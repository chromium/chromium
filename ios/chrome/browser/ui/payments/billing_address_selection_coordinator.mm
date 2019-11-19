// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/billing_address_selection_coordinator.h"

#include <vector>

#include "base/logging.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/ui/payments/billing_address_selection_mediator.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The delay in nano seconds before notifying the delegate of the selection.
const int64_t kDelegateNotificationDelayInNanoSeconds = 0.2 * NSEC_PER_SEC;
}  // namespace

@interface BillingAddressSelectionCoordinator ()

@property(nonatomic, strong) AddressEditCoordinator* addressEditCoordinator;

@property(nonatomic, strong)
    PaymentRequestSelectorViewController* viewController;

@property(nonatomic, strong) BillingAddressSelectionMediator* mediator;

// Initializes and starts the AddressEditCoordinator. Sets |address| as the
// address to be edited.
- (void)startAddressEditCoordinatorWithAddress:
    (autofill::AutofillProfile*)address;

// Called when the user selects a billing address. The cell is checked, the
// UI is locked so that the user can't interact with it, then the delegate is
// notified. The delay is here to let the user get a visual feedback of the
// selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection:
    (autofill::AutofillProfile*)billingAddress;

@end

@implementation BillingAddressSelectionCoordinator

@synthesize selectedBillingProfile = _selectedBillingProfile;
@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize addressEditCoordinator = _addressEditCoordinator;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

- (void)start {
  self.mediator = [[BillingAddressSelectionMediator alloc]
      initWithPaymentRequest:self.paymentRequest
      selectedBillingProfile:self.selectedBillingProfile];

  self.viewController = [[PaymentRequestSelectorViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.dataSource = self.mediator;
  [self.viewController loadModel];

  DCHECK(self.baseViewController.navigationController);
  [self.baseViewController.navigationController
      pushViewController:self.viewController
                animated:YES];
}

- (void)stop {
  [self.viewController.navigationController popViewControllerAnimated:YES];
  [self.addressEditCoordinator stop];
  self.addressEditCoordinator = nil;
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - PaymentRequestSelectorViewControllerDelegate

- (BOOL)paymentRequestSelectorViewController:
            (PaymentRequestSelectorViewController*)controller
                        didSelectItemAtIndex:(NSUInteger)index {
  CollectionViewItem<PaymentsIsSelectable>* selectedItem =
      self.mediator.selectableItems[index];

  DCHECK(index < self.paymentRequest->billing_profiles().size());
  autofill::AutofillProfile* billingProfile =
      self.paymentRequest->billing_profiles()[index];

  // Proceed with item selection only if the item has all required info, or
  // else bring up the address editor.
  if (selectedItem.complete) {
    // Update the data source with the selection.
    self.mediator.selectedItemIndex = index;
    [self delayedNotifyDelegateOfSelection:billingProfile];
    return YES;
  } else {
    [self startAddressEditCoordinatorWithAddress:billingProfile];
    return NO;
  }
}

- (void)paymentRequestSelectorViewControllerDidFinish:
    (PaymentRequestSelectorViewController*)controller {
  [self.delegate billingAddressSelectionCoordinatorDidReturn:self];
}

- (void)paymentRequestSelectorViewControllerDidSelectAddItem:
    (PaymentRequestSelectorViewController*)controller {
  [self startAddressEditCoordinatorWithAddress:nil];
}

- (void)paymentRequestSelectorViewControllerDidToggleEditingMode:
    (PaymentRequestSelectorViewController*)controller {
  [self.viewController loadModel];
  [self.viewController.collectionView reloadData];
}

- (void)paymentRequestSelectorViewController:
            (PaymentRequestSelectorViewController*)controller
              didSelectItemAtIndexForEditing:(NSUInteger)index {
  DCHECK(index < self.paymentRequest->billing_profiles().size());
  [self startAddressEditCoordinatorWithAddress:self.paymentRequest
                                                   ->billing_profiles()[index]];
}

#pragma mark - AddressEditCoordinatorDelegate

- (void)addressEditCoordinator:(AddressEditCoordinator*)coordinator
       didFinishEditingAddress:(autofill::AutofillProfile*)address {
  // Update the data source with the new data.
  [self.mediator loadItems];

  const std::vector<autofill::AutofillProfile*>& billingProfiles =
      self.paymentRequest->billing_profiles();
  const auto position =
      std::find(billingProfiles.begin(), billingProfiles.end(), address);
  DCHECK(position != billingProfiles.end());

  // Mark the edited item as complete meaning all required information has been
  // filled out.
  CollectionViewItem<PaymentsIsSelectable>* editedItem =
      self.mediator.selectableItems[position - billingProfiles.begin()];
  editedItem.complete = YES;

  if (![self.viewController isEditing]) {
    // Update the data source with the selection.
    self.mediator.selectedItemIndex = position - billingProfiles.begin();
  }

  [self.viewController loadModel];
  [self.viewController.collectionView reloadData];

  [self.addressEditCoordinator stop];
  self.addressEditCoordinator = nil;

  if (![self.viewController isEditing]) {
    // Inform |self.delegate| that |address| has been selected.
    [self.delegate billingAddressSelectionCoordinator:self
                              didSelectBillingAddress:address];
  }
}

- (void)addressEditCoordinatorDidCancel:(AddressEditCoordinator*)coordinator {
  [self.addressEditCoordinator stop];
  self.addressEditCoordinator = nil;
}

#pragma mark - Helper methods

- (void)startAddressEditCoordinatorWithAddress:
    (autofill::AutofillProfile*)address {
  self.addressEditCoordinator = [[AddressEditCoordinator alloc]
      initWithBaseViewController:self.viewController];
  self.addressEditCoordinator.paymentRequest = self.paymentRequest;
  self.addressEditCoordinator.address = address;
  self.addressEditCoordinator.delegate = self;
  [self.addressEditCoordinator start];
}

- (void)delayedNotifyDelegateOfSelection:
    (autofill::AutofillProfile*)billingAddress {
  self.viewController.view.userInteractionEnabled = NO;
  __weak BillingAddressSelectionCoordinator* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kDelegateNotificationDelayInNanoSeconds),
      dispatch_get_main_queue(), ^{
        [weakSelf.delegate billingAddressSelectionCoordinator:weakSelf
                                      didSelectBillingAddress:billingAddress];
      });
}

@end
