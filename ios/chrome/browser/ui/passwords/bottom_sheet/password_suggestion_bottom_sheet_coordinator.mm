// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_coordinator.h"

#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_controller_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/scoped_password_suggestion_bottom_sheet_reauth_module_override.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/web/public/web_state.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using PasswordSuggestionBottomSheetExitReason::kShowPasswordDetails;
using PasswordSuggestionBottomSheetExitReason::kShowPasswordManager;

@interface PasswordSuggestionBottomSheetCoordinator () {
  // The password controller delegate used to open the password manager.
  id<PasswordControllerDelegate> _passwordControllerDelegate;
}

// This mediator is used to fetch data related to the bottom sheet.
@property(nonatomic, strong) PasswordSuggestionBottomSheetMediator* mediator;

// This view controller is used to display the bottom sheet.
@property(nonatomic, strong)
    PasswordSuggestionBottomSheetViewController* viewController;

// Module handling reauthentication before accessing sensitive data.
@property(nonatomic, strong) id<ReauthenticationProtocol> reauthModule;

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

    auto profilePasswordStore =
        IOSChromePasswordStoreFactory::GetForBrowserState(
            browserState, ServiceAccessType::EXPLICIT_ACCESS);
    auto accountPasswordStore =
        IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
            browserState, ServiceAccessType::EXPLICIT_ACCESS);

    WebStateList* webStateList = browser->GetWebStateList();
    const GURL& URL = webStateList->GetActiveWebState()->GetLastCommittedURL();

    self.reauthModule =
        ScopedPasswordSuggestionBottomSheetReauthModuleOverride::instance
            ? ScopedPasswordSuggestionBottomSheetReauthModuleOverride::instance
                  ->module
            : [[ReauthenticationModule alloc] init];
    self.mediator = [[PasswordSuggestionBottomSheetMediator alloc]
        initWithWebStateList:webStateList
               faviconLoader:IOSChromeFaviconLoaderFactory::GetForBrowserState(
                                 browserState)
                 prefService:browserState->GetPrefs()
                      params:params
                reauthModule:_reauthModule
                         URL:URL
        profilePasswordStore:profilePasswordStore
        accountPasswordStore:accountPasswordStore];
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
    [self.mediator dismiss];
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
  [self.mediator logExitReason:kShowPasswordManager];

  __weak __typeof(self) weakSelf = self;
  [self.baseViewController.presentedViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           [weakSelf stop];
                         }];

  [_passwordControllerDelegate displaySavedPasswordList];
}

- (void)displayPasswordDetailsForFormSuggestion:
    (FormSuggestion*)formSuggestion {
  [self.mediator logExitReason:kShowPasswordDetails];
  absl::optional<password_manager::CredentialUIEntry> credential =
      [self.mediator getCredentialForFormSuggestion:formSuggestion];

  __weak __typeof(self) weakSelf = self;
  [self.baseViewController.presentedViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           [weakSelf stop];
                         }];

  if (credential.has_value()) {
    [_passwordControllerDelegate
        showPasswordDetailsForCredential:credential.value()];
  }
  // TODO(crbug.com/1422344): Add metric for when the credential is nil.
}

@end
