// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_edit_profile_coordinator.h"

#import <Foundation/Foundation.h>

#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_country_selection_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_edit_profile_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_edit_profile_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_table_view_helper.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/country_item.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface AutofillEditProfileCoordinator () <
    AutofillCountrySelectionTableViewControllerDelegate,
    AutofillEditProfileTableViewControllerDelegate,
    AutofillProfileEditMediatorDelegate,
    UIAdaptivePresentationControllerDelegate>

@end

@implementation AutofillEditProfileCoordinator {
  // Profile to be edited.
  std::unique_ptr<autofill::AutofillProfile> _autofillProfile;

  // Navigation controller presented by this coordinator.
  TableViewNavigationController* _navigationController;

  // TVC for displaying the bottom sheet.
  AutofillEditProfileTableViewController* _viewController;

  // Mediator and view controller used to display the edit view.
  AutofillProfileEditTableViewHelper* _AutofillProfileEditTableViewHelper;
  AutofillProfileEditMediator* _autofillProfileEditMediator;

  raw_ptr<autofill::PersonalDataManager> _personalDataManager;

  // The action sheet coordinator, if one is currently being shown.
  ActionSheetCoordinator* _actionSheetCoordinator;

  // Handler that manages the process of saving addresses, either by creating a
  // new address record or by editing an existing one before saving, depending
  // on the context in which this coordinator is used.
  __weak id<AutofillEditProfileHandler> _handler;
}

- (instancetype)
    initWithBaseViewController:(UINavigationController*)viewController
                       browser:(Browser*)browser
                       handler:(id<AutofillEditProfileHandler>)handler {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    ProfileIOS* profile = browser->GetProfile();

    // Address Save Prompt is not shown in the incognito mode.
    CHECK(!profile->IsOffTheRecord());
    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile);

    CHECK(handler);
    _handler = handler;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _autofillProfile = [_handler autofillProfile];

  _autofillProfileEditMediator = [[AutofillProfileEditMediator alloc]
         initWithDelegate:self
      personalDataManager:_personalDataManager
          autofillProfile:_autofillProfile.get()
        isMigrationPrompt:[_handler isMigrationToAccount]
         addManualAddress:[_handler addingManualAddress]];

  // Bottom sheet table VC
  AutofillEditProfileTableViewController* editModalViewController =
      [[AutofillEditProfileTableViewController alloc]
          initWithDelegate:self
             editSheetMode:[_handler saveProfilePromptMode]];

  SaveAddressContext saveAddressContext =
      [_handler addingManualAddress]
          ? SaveAddressContext::kAddingManualAddress
          : SaveAddressContext::kInfobarSaveUpdateAddress;

  // View controller that lays down the table views for the edit profile view.
  _AutofillProfileEditTableViewHelper =
      [[AutofillProfileEditTableViewHelper alloc]
          initWithDelegate:_autofillProfileEditMediator
                 userEmail:[_handler userEmail]
                controller:editModalViewController
            addressContext:saveAddressContext];
  _autofillProfileEditMediator.consumer = _AutofillProfileEditTableViewHelper;
  // `editModalViewController` lays down the bottom sheet view and communicates
  // with `_AutofillProfileEditTableViewHelper` via
  // `AutofillProfileEditHandler` protocol.
  // `_AutofillProfileEditTableViewHelper` is responsible for loading the
  // model and dealing with the table view user interactions.
  editModalViewController.handler = _AutofillProfileEditTableViewHelper;

  _viewController = editModalViewController;

  _navigationController =
      [[TableViewNavigationController alloc] initWithTable:_viewController];
  BOOL isIPad =
      UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
  _navigationController.modalPresentationStyle =
      isIPad ? UIModalPresentationFormSheet : UIModalPresentationPageSheet;
  _navigationController.modalTransitionStyle =
      UIModalTransitionStyleCoverVertical;
  _navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  // TODO(crbug.com/406514222): Coordinator's parent should be calling this
  // method.
  [super stop];
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController.presentationController.delegate = nil;
  _viewController = nil;
  _autofillProfileEditMediator = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return [_autofillProfileEditMediator canDismissImmediately];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self didCancelBottomSheetView];
}

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  if (![_autofillProfileEditMediator
          shouldShowConfirmationDialogOnDismissBySwiping]) {
    return;
  }

  _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:_navigationController.topViewController
                         browser:self.browser
                           title:nil
                         message:nil
                   barButtonItem:_navigationController.topViewController
                                     .navigationItem.leftBarButtonItem];

  __weak __typeof(self) weakSelf = self;
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_SAVE_CHANGES)
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                  AutofillEditProfileCoordinator* strongSelf = weakSelf;
                  if (strongSelf) {
                    [strongSelf->_autofillProfileEditMediator
                            saveChangesForDismiss];
                  }
                }
                 style:UIAlertActionStyleDefault];

  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                  [weakSelf didCancelBottomSheetView];
                }
                 style:UIAlertActionStyleDestructive];

  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleCancel];

  [_actionSheetCoordinator start];
}

#pragma mark - AutofillProfileEditMediatorDelegate

- (void)autofillEditProfileMediatorDidFinish:
    (AutofillProfileEditMediator*)mediator {
}

- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country
                                          countryList:(NSArray<CountryItem*>*)
                                                          allCountries {
  AutofillCountrySelectionTableViewController*
      autofillCountrySelectionTableViewController =
          [[AutofillCountrySelectionTableViewController alloc]
                         initWithDelegate:self
                          selectedCountry:country
                             allCountries:allCountries
                             settingsView:NO
              previousViewControllerTitle:_navigationController
                                              .topViewController.title];

  [_navigationController
      pushViewController:autofillCountrySelectionTableViewController
                animated:YES];
}

- (void)didSaveProfile {
  [_handler didSaveProfile:_autofillProfile.get()];
  [self stop];
}

#pragma mark - AutofillCountrySelectionTableViewControllerDelegate

- (void)didSelectCountry:(CountryItem*)selectedCountry {
  [_navigationController popViewControllerAnimated:YES];
  [_autofillProfileEditMediator didSelectCountry:selectedCountry];
}

- (void)dismissCountryViewController {
  [_navigationController popViewControllerAnimated:YES];
}

#pragma mark - AutofillEditProfileTableViewControllerDelegate

- (void)didCancelBottomSheetView {
  [_handler didCancelSheetView];

  [self stop];
}

#pragma mark - Private

- (void)dismissActionSheetCoordinator {
  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = nil;
}

@end
