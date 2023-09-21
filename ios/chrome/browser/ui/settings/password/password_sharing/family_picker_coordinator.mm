// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
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
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

@interface FamilyPickerCoordinator () <
    FamilyPickerViewControllerPresentationDelegate> {
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

@implementation FamilyPickerCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                          recipients:(NSArray<RecipientInfoForIOSDisplay*>*)
                                         recipients {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _recipients = recipients;
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
      sharedURLLoaderFactory:self.browser->GetBrowserState()
                                 ->GetSharedURLLoaderFactory()];
  self.mediator.consumer = self.viewController;
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

  if (self.shouldNavigateBack) {
    [self.viewController setupLeftBackButton];
  } else {
    [self.viewController setupLeftCancelButton];
  }

  // Disable animation when the view is displayed on top of the spinner view so
  // that it looks as the spinner is replaced with the loaded data.
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:self.shouldNavigateBack];
}

- (void)stop {
  self.navigationController = nil;
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - FamilyPickerViewControllerPresentationDelegate

- (void)familyPickerWasDismissed:(FamilyPickerViewController*)controller {
  [self.delegate familyPickerCoordinatorWasDismissed:self];
}

- (void)familyPickerClosed:(FamilyPickerViewController*)controller
    withSelectedRecipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients {
  [self.delegate familyPickerCoordinator:self didSelectRecipients:recipients];
}

- (void)familyPickerNavigatedBack:(FamilyPickerViewController*)controller {
  [self.baseNavigationController popViewControllerAnimated:YES];
  [self.delegate familyPickerCoordinatorNavigatedBack:self];
}

- (void)learnMoreLinkWasTapped {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:GURL(kPasswordSharingLearnMoreURL)];
  [handler closeSettingsUIAndOpenURL:command];
  [self.delegate familyPickerCoordinatorWasDismissed:self];
}

@end
