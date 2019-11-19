// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/contact_info_selection_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_is_selectable.h"
#include "ios/chrome/browser/ui/payments/contact_info_selection_mediator.h"
#include "ios/chrome/browser/ui/payments/payment_request_selector_view_controller.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The delay in nano seconds before notifying the delegate of the selection.
// This is here to let the user get a visual feedback of the selection before
// this view disappears.
const int64_t kDelegateNotificationDelayInNanoSeconds = 0.2 * NSEC_PER_SEC;
}  // namespace

@interface ContactInfoSelectionCoordinator ()

@property(nonatomic, strong)
    ContactInfoEditCoordinator* contactInfoEditCoordinator;

@property(nonatomic, strong)
    PaymentRequestSelectorViewController* viewController;

@property(nonatomic, strong) ContactInfoSelectionMediator* mediator;

// Initializes and starts the ContactInfoEditCoordinator. Sets |profile| as the
// profile to be edited.
- (void)startContactInfoEditCoordinatorWithProfile:
    (autofill::AutofillProfile*)profile;

// Called when the user selects a contact profile. The cell is checked, the
// UI is locked so that the user can't interact with it, then the delegate is
// notified.
- (void)delayedNotifyDelegateOfSelection:
    (autofill::AutofillProfile*)contactProfile;

@end

@implementation ContactInfoSelectionCoordinator

@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize contactInfoEditCoordinator = _contactInfoEditCoordinator;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

- (void)start {
  self.mediator = [[ContactInfoSelectionMediator alloc]
      initWithPaymentRequest:self.paymentRequest];

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
  [self.baseViewController.navigationController popViewControllerAnimated:YES];
  [self.contactInfoEditCoordinator stop];
  self.contactInfoEditCoordinator = nil;
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - PaymentRequestSelectorViewControllerDelegate

- (BOOL)paymentRequestSelectorViewController:
            (PaymentRequestSelectorViewController*)controller
                        didSelectItemAtIndex:(NSUInteger)index {
  CollectionViewItem<PaymentsIsSelectable>* selectedItem =
      self.mediator.selectableItems[index];

  DCHECK(index < self.paymentRequest->contact_profiles().size());
  autofill::AutofillProfile* selectedProfile =
      self.paymentRequest->contact_profiles()[index];

  // Proceed with item selection only if the item has all required info, or
  // else bring up the contact info editor.
  if (selectedItem.complete) {
    // Update the data source with the selection.
    self.mediator.selectedItemIndex = index;
    [self delayedNotifyDelegateOfSelection:selectedProfile];
    return YES;
  } else {
    [self startContactInfoEditCoordinatorWithProfile:selectedProfile];
    return NO;
  }
}

- (void)paymentRequestSelectorViewControllerDidFinish:
    (PaymentRequestSelectorViewController*)controller {
  [self.delegate contactInfoSelectionCoordinatorDidReturn:self];
}

- (void)paymentRequestSelectorViewControllerDidSelectAddItem:
    (PaymentRequestSelectorViewController*)controller {
  [self startContactInfoEditCoordinatorWithProfile:nil];
}

- (void)paymentRequestSelectorViewControllerDidToggleEditingMode:
    (PaymentRequestSelectorViewController*)controller {
  [self.viewController loadModel];
  [self.viewController.collectionView reloadData];
}

- (void)paymentRequestSelectorViewController:
            (PaymentRequestSelectorViewController*)controller
              didSelectItemAtIndexForEditing:(NSUInteger)index {
  DCHECK(index < self.paymentRequest->contact_profiles().size());
  [self startContactInfoEditCoordinatorWithProfile:
            self.paymentRequest->contact_profiles()[index]];
}

#pragma mark - ContactInfoEditCoordinatorDelegate

- (void)contactInfoEditCoordinator:(ContactInfoEditCoordinator*)coordinator
           didFinishEditingProfile:(autofill::AutofillProfile*)profile {
  BOOL isEditing = [self.viewController isEditing];

  // Update the data source with the new data.
  [self.mediator loadItems];

  const std::vector<autofill::AutofillProfile*>& contactProfiles =
      self.paymentRequest->contact_profiles();
  const auto position =
      std::find(contactProfiles.begin(), contactProfiles.end(), profile);
  DCHECK(position != contactProfiles.end());

  const size_t index = position - contactProfiles.begin();

  // Mark the edited item as complete meaning all required information has been
  // filled out.
  CollectionViewItem<PaymentsIsSelectable>* editedItem =
      self.mediator.selectableItems[index];
  editedItem.complete = YES;

  if (!isEditing) {
    // Update the data source with the selection.
    self.mediator.selectedItemIndex = index;
  }

  // Exit 'edit' mode, if applicable.
  [self.viewController setEditing:NO];

  [self.viewController loadModel];
  [self.viewController.collectionView reloadData];

  [self.contactInfoEditCoordinator stop];
  self.contactInfoEditCoordinator = nil;

  if (!isEditing) {
    // Inform |self.delegate| that |profile| has been selected.
    [self.delegate contactInfoSelectionCoordinator:self
                           didSelectContactProfile:profile];
  }
}

- (void)contactInfoEditCoordinatorDidCancel:
    (ContactInfoEditCoordinator*)coordinator {
  // Exit 'edit' mode, if applicable.
  if ([self.viewController isEditing]) {
    [self.viewController setEditing:NO];
    [self.viewController loadModel];
    [self.viewController.collectionView reloadData];
  }

  [self.contactInfoEditCoordinator stop];
  self.contactInfoEditCoordinator = nil;
}

#pragma mark - Helper methods

- (void)startContactInfoEditCoordinatorWithProfile:
    (autofill::AutofillProfile*)profile {
  self.contactInfoEditCoordinator = [[ContactInfoEditCoordinator alloc]
      initWithBaseViewController:self.viewController];
  self.contactInfoEditCoordinator.paymentRequest = self.paymentRequest;
  self.contactInfoEditCoordinator.profile = profile;
  self.contactInfoEditCoordinator.delegate = self;
  [self.contactInfoEditCoordinator start];
}

- (void)delayedNotifyDelegateOfSelection:
    (autofill::AutofillProfile*)contactProfile {
  self.viewController.view.userInteractionEnabled = NO;
  __weak ContactInfoSelectionCoordinator* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kDelegateNotificationDelayInNanoSeconds),
      dispatch_get_main_queue(), ^{
        [weakSelf.delegate contactInfoSelectionCoordinator:weakSelf
                                   didSelectContactProfile:contactProfile];
      });
}

@end
