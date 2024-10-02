// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

@interface FamilyPickerCoordinator () <
    FamilyPickerViewControllerPresentationDelegate,
    UIAdaptivePresentationControllerDelegate> {
  NSArray<RecipientInfoForIOSDisplay*>* _recipients;
}

// The navigation controller displaying the view controller.
@property(nonatomic, strong)
    TableViewNavigationController* navigationController;

// Main view controller for this coordinator.
@property(nonatomic, strong) FamilyPickerViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) FamilyPickerMediator* mediator;

@end

@implementation FamilyPickerCoordinator {
  // Whether the view should have a back button that navigates to the password
  // picker view.
  BOOL _shouldNavigateBack;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                recipients:
                                    (NSArray<RecipientInfoForIOSDisplay*>*)
                                        recipients {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _recipients = recipients;
  }
  return self;
}

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                          recipients:(NSArray<RecipientInfoForIOSDisplay*>*)
                                         recipients {
  self = [self initWithBaseViewController:navigationController
                                  browser:browser
                               recipients:recipients];
  if (self) {
    _baseNavigationController = navigationController;
    _shouldNavigateBack = YES;
  }
  return self;
}

- (void)start {
  [super start];

  self.viewController =
      [[FamilyPickerViewController alloc] initWithStyle:ChromeTableViewStyle()];
  self.viewController.delegate = self;
  self.mediator = [[FamilyPickerMediator alloc]
          initWithRecipients:_recipients
      sharedURLLoaderFactory:self.browser->GetProfile()
                                 ->GetSharedURLLoaderFactory()];
  self.mediator.consumer = self.viewController;

  if (_shouldNavigateBack) {
    [self.viewController setupLeftBackButton];

    CHECK(self.baseNavigationController);
    self.baseNavigationController.presentationController.delegate = self;
    [self.baseNavigationController pushViewController:self.viewController
                                             animated:YES];

  } else {
    [self.viewController setupLeftCancelButton];

    self.navigationController = [[TableViewNavigationController alloc]
        initWithTable:self.viewController];
    [self.navigationController
        setModalPresentationStyle:UIModalPresentationFormSheet];
    self.navigationController.navigationBar.prefersLargeTitles = NO;
    self.navigationController.sheetPresentationController.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];

    self.navigationController.presentationController.delegate = self;

    [self.baseViewController presentViewController:self.navigationController
                                          animated:YES
                                        completion:nil];
  }

  LogPasswordSharingInteraction(
      PasswordSharingInteraction::kFamilyPickerOpened);
}

- (void)stop {
  if (!_shouldNavigateBack) {
    [self.viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
  self.viewController = nil;
  self.navigationController = nil;
  self.mediator = nil;
}

#pragma mark - FamilyPickerViewControllerPresentationDelegate

- (void)familyPickerWasDismissed:(FamilyPickerViewController*)controller {
  [self.delegate familyPickerCoordinatorWasDismissed:self];
}

- (void)familyPickerClosed:(FamilyPickerViewController*)controller
    withSelectedRecipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients {
  LogPasswordSharingInteraction(
      recipients.count == 1
          ? PasswordSharingInteraction::kFamilyPickerShareWithOneMember
          : PasswordSharingInteraction::kFamilyPickerShareWithMultipleMembers);

  __weak __typeof(self.delegate) weakDelegate = self.delegate;
  __weak __typeof(self) weakSelf = self;
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakDelegate familyPickerCoordinator:weakSelf
                                             didSelectRecipients:recipients];
                         }];
}

- (void)familyPickerNavigatedBack:(FamilyPickerViewController*)controller {
  [self.baseNavigationController popViewControllerAnimated:YES];
  [self.delegate familyPickerCoordinatorNavigatedBack:self];
}

- (void)learnMoreLinkWasTapped {
  LogPasswordSharingInteraction(
      PasswordSharingInteraction::
          kFamilyPickerIneligibleRecipientLearnMoreClicked);

  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:GURL(kPasswordSharingLearnMoreURL)];
  [handler closeSettingsUIAndOpenURL:command];
  [self.delegate familyPickerCoordinatorWasDismissed:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate familyPickerCoordinatorWasDismissed:self];
}

@end
