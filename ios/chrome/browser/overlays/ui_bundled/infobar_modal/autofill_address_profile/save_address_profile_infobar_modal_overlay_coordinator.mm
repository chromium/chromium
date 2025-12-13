// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_coordinator.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/autofill_address_profile/infobar_save_address_profile_view_controller.h"
#import "ios/chrome/browser/overlays/model/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;

@interface SaveAddressProfileInfobarModalOverlayCoordinator () <
    SaveAddressProfileInfobarModalOverlayMediatorDelegate>

// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, strong, readwrite)
    SaveAddressProfileInfobarModalOverlayMediator* modalMediator;

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
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  AutofillBottomSheetTabHelper* bottomSheetTabHelper =
      AutofillBottomSheetTabHelper::FromWebState(webState);
  bottomSheetTabHelper->ShowEditAddressBottomSheet();
}

@end

@implementation
SaveAddressProfileInfobarModalOverlayCoordinator (ModalConfiguration)

- (void)configureModal {
  CHECK(!self.modalMediator);
  CHECK(!self.modalViewController);
  SaveAddressProfileInfobarModalOverlayMediator* modalMediator =
      [[SaveAddressProfileInfobarModalOverlayMediator alloc]
          initWithRequest:self.request];
  InfobarSaveAddressProfileViewController* modalViewController =
      [[InfobarSaveAddressProfileViewController alloc]
          initWithModalDelegate:modalMediator];
  modalMediator.consumer = modalViewController;
  modalMediator.saveAddressProfileMediatorDelegate = self;
  self.modalMediator = modalMediator;
  self.modalViewController = modalViewController;
}

- (void)resetModal {
  CHECK(self.modalMediator);
  CHECK(self.modalViewController);
  self.modalMediator = nil;
  self.modalViewController = nil;
}

@end
