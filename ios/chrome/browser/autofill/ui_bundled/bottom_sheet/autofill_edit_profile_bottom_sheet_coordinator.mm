// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/autofill_edit_profile_bottom_sheet_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_country_selection_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/autofill_edit_profile_bottom_sheet_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/country_item.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"

@interface AutofillEditProfileBottomSheetCoordinator () <
    AutofillCountrySelectionTableViewControllerDelegate,
    AutofillEditProfileBottomSheetTableViewControllerDelegate,
    AutofillProfileEditMediatorDelegate>
@end

@implementation AutofillEditProfileBottomSheetCoordinator {
  // Profile to be edited.
  std::unique_ptr<autofill::AutofillProfile> _autofillProfile;

  // Navigation controller presented by this coordinator.
  TableViewNavigationController* _navigationController;

  // TVC for displaying the bottom sheet.
  AutofillEditProfileBottomSheetTableViewController* _viewController;

  // Mediator and view controller used to display the edit view.
  AutofillProfileEditTableViewController*
      _autofillProfileEditTableViewController;
  AutofillProfileEditMediator* _autofillProfileEditMediator;

  raw_ptr<autofill::PersonalDataManager> _personalDataManager;
  raw_ptr<web::WebState> _webState;
}

- (instancetype)initWithBaseViewController:
                    (UINavigationController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    ProfileIOS* profile = browser->GetProfile();

    // Address Save Prompt is not shown in the incognito mode.
    CHECK(!profile->IsOffTheRecord());
    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile);

    _webState = browser->GetWebStateList()->GetActiveWebState();
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      [self fetchDelegate];
  CHECK(delegate);
  _autofillProfile =
      std::make_unique<autofill::AutofillProfile>(*delegate->GetProfile());

  AutofillSaveProfilePromptMode saveProfilePromptMode =
      AutofillSaveProfilePromptMode::kNewProfile;
  if (delegate->IsMigrationToAccount()) {
    saveProfilePromptMode = AutofillSaveProfilePromptMode::kMigrateProfile;
  } else if (delegate->GetOriginalProfile() != nullptr) {
    saveProfilePromptMode = AutofillSaveProfilePromptMode::kUpdateProfile;
  }

  _autofillProfileEditMediator = [[AutofillProfileEditMediator alloc]
         initWithDelegate:self
      personalDataManager:_personalDataManager
          autofillProfile:_autofillProfile.get()
        isMigrationPrompt:delegate->IsMigrationToAccount()];

  // Bottom sheet table VC
  AutofillEditProfileBottomSheetTableViewController* editModalViewController =
      [[AutofillEditProfileBottomSheetTableViewController alloc]
          initWithDelegate:self
             editSheetMode:saveProfilePromptMode];

  // View controller that lays down the table views for the edit profile view.
  _autofillProfileEditTableViewController =
      [[AutofillProfileEditTableViewController alloc]
          initWithDelegate:_autofillProfileEditMediator
                 userEmail:(delegate->UserAccountEmail()
                                ? base::SysUTF16ToNSString(
                                      delegate->UserAccountEmail().value())
                                : nil)controller:editModalViewController
              settingsView:NO];
  _autofillProfileEditMediator.consumer =
      _autofillProfileEditTableViewController;
  // `editModalViewController` lays down the bottom sheet view and communicates
  // with `_autofillProfileEditTableViewController` via
  // `AutofillProfileEditHandler` protocol.
  // `_autofillProfileEditTableViewController` is responsible for loading the
  // model and dealing with the table view user interactions.
  editModalViewController.handler = _autofillProfileEditTableViewController;

  _viewController = editModalViewController;

  _navigationController =
      [[TableViewNavigationController alloc] initWithTable:_viewController];
  BOOL isIPad =
      UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
  if (isIPad) {
    _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
    _navigationController.modalInPresentation = YES;
  } else {
    _navigationController.modalPresentationStyle =
        UIModalPresentationFullScreen;
  }

  _navigationController.modalTransitionStyle =
      UIModalTransitionStyleCoverVertical;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _viewController = nil;
  _autofillProfileEditMediator = nil;
}

#pragma mark - AutofillProfileEditMediatorDelegate

- (void)autofillEditProfileMediatorDidFinish:
    (AutofillProfileEditMediator*)mediator {
  // TODO(crbug.com/40281788): Implement.
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
                  settingsView:NO];

  [_navigationController
      pushViewController:autofillCountrySelectionTableViewController
                animated:YES];
}

- (void)didSaveProfile {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      [self fetchDelegateAndAcceptInfobar];

  if (delegate) {
    delegate->SetProfile(_autofillProfile.get());
    delegate->EditAccepted();
  }

  [self stop];
}

#pragma mark - AutofillCountrySelectionTableViewControllerDelegate

- (void)didSelectCountry:(CountryItem*)selectedCountry {
  [_navigationController popViewControllerAnimated:YES];
  [_autofillProfileEditMediator didSelectCountry:selectedCountry];
}

#pragma mark - AutofillEditProfileBottomSheetTableViewControllerDelegate

- (void)didCancelBottomSheetView {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      [self fetchDelegate];
  if (delegate->IsMigrationToAccount()) {
    delegate->Never();
    infobars::InfoBar* infobar = [self addressInfobar];
    if (infobar) {
      InfoBarManagerImpl::FromWebState(_webState)->RemoveInfoBar(infobar);
    }
  } else {
    delegate->EditDeclined();
  }

  [self stop];
}

#pragma mark - Private

- (autofill::AutofillSaveUpdateAddressProfileDelegateIOS*)
    fetchDelegateAndAcceptInfobar {
  InfoBarIOS* infobar = static_cast<InfoBarIOS*>([self addressInfobar]);
  if (!infobar) {
    return nullptr;
  }

  infobar->set_accepted(YES);
  return [self fetchDelegateFromInfobar:infobar];
}

- (autofill::AutofillSaveUpdateAddressProfileDelegateIOS*)fetchDelegate {
  InfoBarIOS* infobar = static_cast<InfoBarIOS*>([self addressInfobar]);
  if (!infobar) {
    return nullptr;
  }

  return [self fetchDelegateFromInfobar:infobar];
}

- (infobars::InfoBar*)addressInfobar {
  if (!_webState) {
    // Stop here if the '_webState' was deleted. Doing anything further is
    // unsafe.
    base::debug::DumpWithoutCrashing();
    return nullptr;
  }

  InfoBarManagerImpl* manager = InfoBarManagerImpl::FromWebState(_webState);
  CHECK(manager);
  const auto it = base::ranges::find(
      manager->infobars(), InfobarType::kInfobarTypeSaveAutofillAddressProfile,
      [](const infobars::InfoBar* infobar) {
        return static_cast<const InfoBarIOS*>(infobar)->infobar_type();
      });

  return it != manager->infobars().cend() ? *it : nullptr;
}

- (autofill::AutofillSaveUpdateAddressProfileDelegateIOS*)
    fetchDelegateFromInfobar:(InfoBarIOS*)infobar {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      autofill::AutofillSaveUpdateAddressProfileDelegateIOS::
          FromInfobarDelegate(infobar->delegate());

  return delegate;
}

@end
