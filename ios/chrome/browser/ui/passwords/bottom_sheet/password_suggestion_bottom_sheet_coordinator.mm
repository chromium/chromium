// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_coordinator.h"

#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_controller_delegate.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordSuggestionBottomSheetCoordinator () {
  // The password controller delegate used to open the password manager.
  id<PasswordControllerDelegate> _passwordControllerDelegate;

  // Service which gives us a view on users' saved passwords.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      _savedPasswordsPresenter;
}

// This mediator is used to fetch data related to the bottom sheet.
@property(nonatomic, strong) PasswordSuggestionBottomSheetMediator* mediator;

// This view controller is used to display the bottom sheet.
@property(nonatomic, strong)
    PasswordSuggestionBottomSheetViewController* viewController;

@end

@implementation PasswordSuggestionBottomSheetCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        params:(const autofill::FormActivityParams&)params
                      delegate:(id<PasswordControllerDelegate>)delegate {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _passwordControllerDelegate = delegate;
    self.viewController = [[PasswordSuggestionBottomSheetViewController alloc]
        initWithHandler:self];

    ChromeBrowserState* browserState =
        browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _savedPasswordsPresenter =
        std::make_unique<password_manager::SavedPasswordsPresenter>(
            IOSChromeAffiliationServiceFactory::GetForBrowserState(
                browserState),
            IOSChromePasswordStoreFactory::GetForBrowserState(
                browserState, ServiceAccessType::EXPLICIT_ACCESS),
            IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
                browserState, ServiceAccessType::EXPLICIT_ACCESS));

    self.mediator = [[PasswordSuggestionBottomSheetMediator alloc]
           initWithWebStateList:browser->GetWebStateList()
                  faviconLoader:IOSChromeFaviconLoaderFactory::
                                    GetForBrowserState(browserState)
                    prefService:browserState->GetPrefs()
                         params:params
        savedPasswordsPresenter:_savedPasswordsPresenter.get()];
    self.viewController.delegate = self.mediator;

    self.mediator.consumer = self.viewController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // If the bottom sheet has no suggestion to show, do not show the bottom
  // sheet. Instead, re-focus the field which triggered the bottom sheet and
  // disable it.
  if (![self.mediator hasSuggestions]) {
    [self.mediator refocus];
    return;
  }

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - PasswordSuggestionBottomSheetHandler

- (void)displayPasswordManager {
  [_passwordControllerDelegate displaySavedPasswordList];
}

@end
