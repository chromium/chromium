// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_coordinator.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_country_selection_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/country_item.h"
#import "ios/chrome/browser/overlays/model/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/legacy_infobar_edit_address_profile_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;

@interface SaveAddressProfileInfobarModalOverlayCoordinator () <
    AutofillCountrySelectionTableViewControllerDelegate,
    AutofillProfileEditMediatorDelegate,
    SaveAddressProfileInfobarModalOverlayMediatorDelegate> {
  std::unique_ptr<autofill::AutofillProfile> _autofillProfile;
}

// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, strong, readwrite)
    SaveAddressProfileInfobarModalOverlayMediator* modalMediator;

@property(nonatomic, strong, readwrite) UIViewController* modalViewController;

// Mediator and view controller used to display the edit view.
@property(nonatomic, strong, readwrite)
    AutofillProfileEditTableViewController* sharedEditViewController;
@property(nonatomic, strong, readwrite)
    AutofillProfileEditMediator* sharedEditViewMediator;

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
  if (base::FeatureList::IsEnabled(
          kAutofillDynamicallyLoadsFieldsForAddressInput)) {
    web::WebState* webState =
        self.browser->GetWebStateList()->GetActiveWebState();
    AutofillBottomSheetTabHelper* bottomSheetTabHelper =
        AutofillBottomSheetTabHelper::FromWebState(webState);
    bottomSheetTabHelper->ShowEditAddressBottomSheet();
    return;
  }

  [self.baseViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           // Shows Edit View Controller.
                           [self onSaveUpdateViewDismissed];
                         }];
}

#pragma mark - Private

- (void)onSaveUpdateViewDismissed {
  if (!self.config) {
    return;
  }

  SaveAddressProfileInfobarModalOverlayMediator* modalMediator =
      static_cast<SaveAddressProfileInfobarModalOverlayMediator*>(
          self.modalMediator);
  _autofillProfile =
      std::make_unique<autofill::AutofillProfile>(*(self.config->GetProfile()));
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          self.browser->GetProfile()->GetOriginalProfile());
  self.sharedEditViewMediator = [[AutofillProfileEditMediator alloc]
         initWithDelegate:self
      personalDataManager:personalDataManager
          autofillProfile:_autofillProfile.get()
        isMigrationPrompt:self.config->is_migration_to_account()];

  LegacyInfobarEditAddressProfileTableViewController* editModalViewController =
      [[LegacyInfobarEditAddressProfileTableViewController alloc]
          initWithModalDelegate:modalMediator];
  self.sharedEditViewController =
      [[AutofillProfileEditTableViewController alloc]
          initWithDelegate:self.sharedEditViewMediator
                 userEmail:(self.config->user_email()
                                ? base::SysUTF16ToNSString(
                                      self.config->user_email().value())
                                : nil)controller:editModalViewController
              settingsView:NO];
  self.sharedEditViewMediator.consumer = self.sharedEditViewController;
  editModalViewController.handler = self.sharedEditViewController;

  modalMediator.editAddressConsumer = editModalViewController;
  self.modalMediator = modalMediator;
  self.modalViewController = editModalViewController;

  [self configureViewController];
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - AutofillProfileEditMediatorDelegate

- (void)autofillEditProfileMediatorDidFinish:
    (AutofillProfileEditMediator*)mediator {
  CHECK(!base::FeatureList::IsEnabled(
      kAutofillDynamicallyLoadsFieldsForAddressInput));
}

- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country
                                          countryList:(NSArray<CountryItem*>*)
                                                          allCountries {
  CHECK(!base::FeatureList::IsEnabled(
      kAutofillDynamicallyLoadsFieldsForAddressInput));
  AutofillCountrySelectionTableViewController*
      autofillCountrySelectionTableViewController =
          [[AutofillCountrySelectionTableViewController alloc]
              initWithDelegate:self
               selectedCountry:country
                  allCountries:allCountries
                  settingsView:NO];

  [self.modalViewController.navigationController
      pushViewController:autofillCountrySelectionTableViewController
                animated:YES];
}

- (void)didSaveProfile {
  CHECK(!base::FeatureList::IsEnabled(
      kAutofillDynamicallyLoadsFieldsForAddressInput));
  [self.modalMediator saveEditedProfileWithProfileData:_autofillProfile.get()];
}

#pragma mark - AutofillCountrySelectionTableViewControllerDelegate

- (void)didSelectCountry:(CountryItem*)selectedCountry {
  CHECK(!base::FeatureList::IsEnabled(
      kAutofillDynamicallyLoadsFieldsForAddressInput));
  [self.modalViewController.navigationController popViewControllerAnimated:YES];
  CHECK(self.sharedEditViewMediator);
  [self.sharedEditViewMediator didSelectCountry:selectedCountry];
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
