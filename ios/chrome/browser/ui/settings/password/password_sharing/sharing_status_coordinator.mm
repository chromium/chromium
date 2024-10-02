// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator.h"

#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_view_controller_presentation_delegate.h"
#import "url/gurl.h"

@interface SharingStatusCoordinator () <
    SharingStatusViewControllerPresentationDelegate,
    UIAdaptivePresentationControllerDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong) SharingStatusViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) SharingStatusMediator* mediator;

@end

@implementation SharingStatusCoordinator {
  // Contains information about the recipients that the user selected to share a
  // password with.
  NSArray<RecipientInfoForIOSDisplay*>* _recipients;

  // Website for which the password is being shared.
  NSString* _website;

  // Url of the site for which the password is being shared.
  GURL _URL;

  // Url which allows to change the password that is being shared. Can be null
  // for Android app credentials.
  std::optional<GURL> _changePasswordURL;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                    recipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients
                       website:(NSString*)website
                           URL:(const GURL&)URL
             changePasswordURL:(const std::optional<GURL>&)changePasswordURL {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _recipients = recipients;
    _website = website;
    _URL = URL;
    _changePasswordURL = changePasswordURL;
  }
  return self;
}

- (void)start {
  [super start];

  self.viewController =
      [[SharingStatusViewController alloc] initWithNibName:nil bundle:nil];
  self.viewController.delegate = self;

  ProfileIOS* profile = self.browser->GetProfile();
  self.mediator = [[SharingStatusMediator alloc]
        initWithAuthService:AuthenticationServiceFactory::GetForProfile(profile)
      accountManagerService:ChromeAccountManagerServiceFactory::GetForProfile(
                                profile)
              faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                profile)
                 recipients:_recipients
                    website:_website
                        URL:_URL
          changePasswordURL:_changePasswordURL];
  self.mediator.consumer = self.viewController;
  self.viewController.imageDataSource = self.mediator;
  self.viewController.presentationController.delegate = self;
  self.viewController.sheetPresentationController.detents = @[
    self.viewController.preferredHeightDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate sharingStatusCoordinatorWasDismissed:self];
}

#pragma mark - SharingStatusViewControllerPresentationDelegate

- (void)sharingStatusWasDismissed:(SharingStatusViewController*)controller {
  LogPasswordSharingInteraction(
      PasswordSharingInteraction::kSharingConfirmationDoneClicked);

  [self.delegate sharingStatusCoordinatorWasDismissed:self];
}

- (void)startPasswordSharing {
  [self.delegate startPasswordSharing];
}

- (void)changePasswordLinkWasTapped {
  CHECK(_changePasswordURL.has_value());

  LogPasswordSharingInteraction(
      PasswordSharingInteraction::kSharingConfirmationChangePasswordClicked);

  [self openURLInNewTabAndCloseSettings:_changePasswordURL.value()];
  [self.delegate sharingStatusCoordinatorWasDismissed:self];
}

#pragma mark - Private

// Opens `URL` in new tab and closes the settings UI.
- (void)openURLInNewTabAndCloseSettings:(const GURL&)URL {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [handler closeSettingsUIAndOpenURL:command];
}

@end
