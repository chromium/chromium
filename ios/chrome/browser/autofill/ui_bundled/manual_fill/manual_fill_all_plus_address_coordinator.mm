// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_all_plus_address_coordinator.h"

#import "components/plus_addresses/plus_address_service.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_all_plus_address_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/plus_address_list_navigator.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/web/public/web_state.h"

@interface ManualFillAllPlusAddressCoordinator () <
    ManualFillAllPlusAddressViewControllerDelegate,
    ManualFillPlusAddressMediatorDelegate,
    PlusAddressListNavigator,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation ManualFillAllPlusAddressCoordinator {
  // Fetches and filters the plus addresses.
  ManualFillPlusAddressMediator* _plusAddressMediator;

  // The view controller.
  ManualFillAllPlusAddressViewController* _plusAddressViewController;

  // Navigation controller presented by this coordinator.
  TableViewNavigationController* _navigationController;
}

- (void)start {
  [super start];
  UISearchController* searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  _plusAddressViewController = [[ManualFillAllPlusAddressViewController alloc]
      initWithSearchController:searchController];
  _plusAddressViewController.delegate = self;

  ProfileIOS* profile = self.browser->GetProfile();
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);

  WebStateList* webStateList = self.browser->GetWebStateList();
  CHECK(webStateList->GetActiveWebState());
  const GURL& URL = webStateList->GetActiveWebState()->GetLastCommittedURL();

  plus_addresses::PlusAddressService* plusAddressService =
      PlusAddressServiceFactory::GetForProfile(profile);
  CHECK(plusAddressService);

  _plusAddressMediator = [[ManualFillPlusAddressMediator alloc]
      initWithFaviconLoader:faviconLoader
         plusAddressService:plusAddressService
                        URL:URL
             isOffTheRecord:profile->IsOffTheRecord()];

  // Fetch all plus addresses before setting the consumer.
  [_plusAddressMediator fetchAllPlusAddresses];
  _plusAddressMediator.contentInjector = self.injectionHandler;
  _plusAddressMediator.delegate = self;

  _plusAddressMediator.consumer = _plusAddressViewController;
  _plusAddressMediator.navigator = self;

  _plusAddressViewController.imageDataSource = _plusAddressMediator;

  searchController.searchResultsUpdater = _plusAddressMediator;

  _navigationController = [[TableViewNavigationController alloc]
      initWithTable:_plusAddressViewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.modalTransitionStyle =
      UIModalTransitionStyleCoverVertical;
  _navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_plusAddressViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _plusAddressViewController = nil;
  _plusAddressMediator.consumer = nil;
  _plusAddressMediator = nil;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.manualFillAllPlusAddressCoordinatorDelegate
      manualFillAllPlusAddressCoordinatorWantsToBeDismissed:self];
}

#pragma mark - ManualFillPlusAddressMediatorDelegate

- (void)manualFillPlusAddressMediatorWillInjectContent {
  [self.manualFillAllPlusAddressCoordinatorDelegate
      manualFillAllPlusAddressCoordinatorWantsToBeDismissed:self];
}

#pragma mark - ManualFillAllPlusAddressViewControllerDelegate

- (void)selectPlusAddressViewControllerDidTapDoneButton:
    (ManualFillAllPlusAddressViewController*)selectPlusAddressViewController {
  [self.manualFillAllPlusAddressCoordinatorDelegate
      manualFillAllPlusAddressCoordinatorWantsToBeDismissed:self];
}

#pragma mark - PlusAddressListNavigator

- (void)openCreatePlusAddressSheet {
  NOTREACHED_NORETURN();
}

- (void)openAllPlusAddressList {
  NOTREACHED_NORETURN();
}

- (void)openManagePlusAddress {
  [self.manualFillAllPlusAddressCoordinatorDelegate
          dismissManualFillAllPlusAddressAndOpenManagePlusAddress];
}

@end
