// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_coordinator.h"

#import "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

using password_manager::FetchFamilyMembersRequestStatus;

@interface PasswordSharingCoordinator () <PasswordSharingMediatorDelegate>

// The navigation controller displaying the view controller.
@property(nonatomic, strong)
    TableViewNavigationController* navigationController;

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordSharingViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordSharingMediator* mediator;

// Coordinator for family picker.
@property(nonatomic, strong) FamilyPickerCoordinator* familyPickerCoordinator;

@end

@implementation PasswordSharingCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

- (void)start {
  [super start];

  self.viewController = [[PasswordSharingViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.navigationController =
      [[TableViewNavigationController alloc] initWithTable:self.viewController];
  [self.navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  UISheetPresentationController* sheetPresentationController =
      self.navigationController.sheetPresentationController;
  if (sheetPresentationController) {
    sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent mediumDetent] ];
  }

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  self.mediator = [[PasswordSharingMediator alloc]
            initWithDelegate:self
      SharedURLLoaderFactory:browserState->GetSharedURLLoaderFactory()
             identityManager:IdentityManagerFactory::GetForBrowserState(
                                 browserState)];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  self.navigationController = nil;
  self.mediator = nil;
}

#pragma mark - PasswordSharingMediatorDelegate

- (void)onFetchFamilyMembers:(NSArray<RecipientInfo*>*)familyMembers
                  withStatus:(const FetchFamilyMembersRequestStatus&)status {
  // TODO(crbug.com/1463882): Add handling multiple credential groups.
  switch (status) {
    case FetchFamilyMembersRequestStatus::kSuccess:
      [self.familyPickerCoordinator stop];
      self.familyPickerCoordinator = [[FamilyPickerCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
      [self.familyPickerCoordinator start];
      break;
    case FetchFamilyMembersRequestStatus::kNoFamily:
      // TODO(crbug.com/1463882): Implement family promo view.
      break;
    case FetchFamilyMembersRequestStatus::kUnknown:
    case FetchFamilyMembersRequestStatus::kNetworkError:
    case FetchFamilyMembersRequestStatus::kPendingRequest:
      // TODO(crbug.com/1463882): Implement error view.
      break;
  }
}

@end
