// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_edit_address_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator_delegate.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;

@interface SaveAddressProfileInfobarModalOverlayCoordinator () <
    SaveAddressProfileInfobarModalOverlayMediatorDelegate>
// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, strong, readwrite) OverlayRequestMediator* modalMediator;
@property(nonatomic, strong, readwrite) UIViewController* modalViewController;
// The request's config.
@property(nonatomic, assign, readonly)
    SaveAddressProfileModalRequestConfig* config;
@end

@implementation SaveAddressProfileInfobarModalOverlayCoordinator

#pragma mark - Accessors

- (SaveAddressProfileModalRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<SaveAddressProfileModalRequestConfig>()
             : nullptr;
}

#pragma mark - Public

+ (const OverlayRequestSupport*)requestSupport {
  return SaveAddressProfileModalRequestConfig::RequestSupport();
}

#pragma mark - SaveAddressProfileInfobarModalOverlayMediatorDelegate

- (void)showEditView {
  [self.baseViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           // Shows Edit View Controller.
                           [self onSaveUpdateViewDismissed];
                         }];
}

#pragma mark - Private

- (void)onSaveUpdateViewDismissed {
  SaveAddressProfileInfobarModalOverlayMediator* modalMediator =
      static_cast<SaveAddressProfileInfobarModalOverlayMediator*>(
          self.modalMediator);
  InfobarEditAddressProfileTableViewController* editModalViewController =
      [[InfobarEditAddressProfileTableViewController alloc]
          initWithModalDelegate:modalMediator];

  modalMediator.editAddressConsumer = editModalViewController;
  self.modalMediator = modalMediator;
  self.modalViewController = editModalViewController;

  [self configureViewController];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

@end

@implementation
    SaveAddressProfileInfobarModalOverlayCoordinator (ModalConfiguration)

- (void)configureModal {
  DCHECK(!self.modalMediator);
  DCHECK(!self.modalViewController);
  SaveAddressProfileInfobarModalOverlayMediator* modalMediator =
      [[SaveAddressProfileInfobarModalOverlayMediator alloc]
          initWithRequest:self.request];
  InfobarSaveAddressProfileTableViewController* modalViewController =
      [[InfobarSaveAddressProfileTableViewController alloc]
          initWithModalDelegate:modalMediator];
  modalMediator.consumer = modalViewController;
  modalMediator.saveAddressProfileMediatorDelegate = self;
  self.modalMediator = modalMediator;
  self.modalViewController = modalViewController;
}

- (void)resetModal {
  DCHECK(self.modalMediator);
  DCHECK(self.modalViewController);
  self.modalMediator = nil;
  self.modalViewController = nil;
}

@end
