// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_coordinator.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "components/autofill/core/browser/at_memory/at_memory_manager.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/personal_context/core/personal_context_prefs.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_coordinator.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_coordinator.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_search_result_commands.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/autofill/public/autofill_settings_navigator.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/web_state.h"

using autofill::AtMemoryManager;
using autofill::AutofillClientIOS;
using autofill::BrowserAutofillManager;
using autofill::FieldGlobalId;
using autofill::autofill_metrics::AutofillSettingsReferrer;

@interface AtMemoryCoordinator () <AtMemorySearchResultCommands,
                                   AutofillSettingsNavigator,
                                   SettingsNavigationControllerDelegate,
                                   UIAdaptivePresentationControllerDelegate>
@end

@implementation AtMemoryCoordinator {
  // NavigationController for the AtMemory flow.
  UINavigationController* _atMemoryNavigationController;
  // NavigationController for Settings.
  SettingsNavigationController* _settingsNavigationController;
  // Injector for manual fill data.
  id<ManualFillContentInjector> _contentInjector;
  // Coordinator for AtMemory search.
  AtMemorySearchCoordinator* _atMemorySearchCoordinator;
  // Coordinator for AtMemory granular fill.
  AtMemoryGranularFillCoordinator* _atMemoryGranularFillCoordinator;
  // Mediator for AtMemory filling.
  AtMemoryMediator* _mediator;
  // Field ID that initiated AtMemory.
  FieldGlobalId _fieldId;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                           contentInjector:
                               (id<ManualFillContentInjector>)contentInjector
                                   fieldId:(FieldGlobalId)fieldId {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _contentInjector = contentInjector;
    _fieldId = fieldId;
  }
  return self;
}

- (BOOL)isSettingsPresented {
  return _settingsNavigationController != nil;
}

- (void)start {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  CHECK(webState);

  AutofillClientIOS* autofillClient = AutofillClientIOS::FromWebState(webState);
  CHECK(autofillClient);

  AtMemoryManager* atMemoryManager = autofillClient->GetAtMemoryManager();
  CHECK(atMemoryManager);

  BrowserAutofillManager* autofillManager =
      static_cast<BrowserAutofillManager*>(
          autofillClient->GetAutofillManagerForPrimaryMainFrame());
  CHECK(autofillManager);

  _mediator = [[AtMemoryMediator alloc] initWithAtMemoryManager:atMemoryManager
                                                autofillManager:autofillManager
                                                contentInjector:_contentInjector
                                                        fieldId:_fieldId];
  _mediator.atMemoryHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AtMemoryCommands);

  _atMemoryNavigationController = [[UINavigationController alloc] init];
  _atMemoryNavigationController.presentationController.delegate = self;

  _atMemorySearchCoordinator = [[AtMemorySearchCoordinator alloc]
      initWithBaseNavigationController:_atMemoryNavigationController
                               browser:self.browser
                               fieldId:_fieldId];
  _atMemorySearchCoordinator.searchResultHandler = self;
  _atMemorySearchCoordinator.fillHandler = _mediator;
  _atMemorySearchCoordinator.settingsNavigator = self;
  [_atMemorySearchCoordinator start];

  _atMemoryNavigationController.modalPresentationStyle =
      UIModalPresentationPageSheet;
  UISheetPresentationController* sheet =
      _atMemoryNavigationController.sheetPresentationController;
  if (sheet) {
    sheet.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];
    sheet.prefersGrabberVisible = YES;
    sheet.prefersScrollingExpandsWhenScrolledToEdge = YES;
    sheet.prefersEdgeAttachedInCompactHeight = YES;
  }

  [self.baseViewController presentViewController:_atMemoryNavigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_settingsNavigationController cleanUpSettings];
  _settingsNavigationController = nil;

  [_atMemoryGranularFillCoordinator stop];
  _atMemoryGranularFillCoordinator = nil;

  [_atMemorySearchCoordinator stop];
  _atMemorySearchCoordinator = nil;

  [_mediator disconnect];
  _mediator = nil;

  [_atMemoryNavigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _atMemoryNavigationController = nil;
}

#pragma mark - AtMemorySearchResultCommands

- (void)showAtMemoryGranularFill:(const autofill::Suggestion&)suggestion {
  [_atMemoryGranularFillCoordinator stop];

  _atMemoryGranularFillCoordinator = [[AtMemoryGranularFillCoordinator alloc]
      initWithBaseNavigationController:_atMemoryNavigationController
                               browser:self.browser
                            suggestion:suggestion];
  _atMemoryGranularFillCoordinator.fillHandler = _mediator;
  _atMemoryGranularFillCoordinator.settingsNavigator = self;
  [_atMemoryGranularFillCoordinator start];
}

#pragma mark - AutofillSettingsNavigator

- (void)openSettingsForPage:(AutofillSettingsPage)page {
  if (_settingsNavigationController) {
    return;
  }

  switch (page) {
    case AutofillSettingsPage::kAddresses:
      _settingsNavigationController = [SettingsNavigationController
          autofillProfileControllerForBrowser:self.browser
                                     delegate:self];
      break;
    case AutofillSettingsPage::kCreditCards:
      _settingsNavigationController = [SettingsNavigationController
          autofillCreditCardControllerForBrowser:self.browser
                                        delegate:self];
      break;
    case AutofillSettingsPage::kIdentityDocs:
      _settingsNavigationController = [SettingsNavigationController
          identityDocsControllerForBrowser:self.browser
                                  referrer:AutofillSettingsReferrer::
                                               kFillingFlowDropdown
                                  delegate:self];
      break;
    case AutofillSettingsPage::kShopping:
      _settingsNavigationController = [SettingsNavigationController
          shoppingControllerForBrowser:self.browser
                              referrer:AutofillSettingsReferrer::
                                           kFillingFlowDropdown
                              delegate:self];
      break;
    case AutofillSettingsPage::kTravel:
      _settingsNavigationController = [SettingsNavigationController
          travelControllerForBrowser:self.browser
                            referrer:AutofillSettingsReferrer::
                                         kFillingFlowDropdown
                            delegate:self];
      break;
    case AutofillSettingsPage::kSuggestionsFromGemini:
      _settingsNavigationController = [SettingsNavigationController
          geminiSuggestionsControllerForBrowser:self.browser
                                       delegate:self];
      break;
    case AutofillSettingsPage::kSuggestionsFromGeminiHelpImprove:
      _settingsNavigationController = [SettingsNavigationController
          geminiHelpImproveControllerForBrowser:self.browser
                                       delegate:self];
      break;
    case AutofillSettingsPage::kPasswordManager:
    case AutofillSettingsPage::kPasswordSettings:
      NOTREACHED();
  }

  [_atMemoryNavigationController
      presentViewController:_settingsNavigationController
                   animated:YES
                 completion:nil];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [_settingsNavigationController cleanUpSettings];
  UIViewController* presentingViewController =
      _settingsNavigationController.presentingViewController;
  __weak __typeof(self) weakSelf = self;
  if (presentingViewController) {
    [presentingViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf onSettingsDismissed];
                           }];
  } else {
    [self onSettingsDismissed];
  }
}

- (void)settingsWasDismissed {
  [_settingsNavigationController cleanUpSettings];
  [self onSettingsDismissed];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  id<AtMemoryCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AtMemoryCommands);
  [handler dismissAtMemory];
}

#pragma mark - Private

// Handles cleanup after Settings is dismissed and closes AtMemory if the 'Find
// and fill with Gemini' toggle or AtMemory is no longer enabled.
- (void)onSettingsDismissed {
  _settingsNavigationController = nil;
  if (!self.browser) {
    return;
  }
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    return;
  }
  AutofillClientIOS* autofillClient = AutofillClientIOS::FromWebState(webState);
  if (!autofillClient) {
    return;
  }
  PrefService* prefService = self.browser->GetProfile()->GetPrefs();

  // TODO(crbug.com/566222022): Remove this check once
  // `kAtMemorySkipEnablementChecks` is no longer required for local
  // development.
  const bool isEnabled =
      base::FeatureList::IsEnabled(
          autofill::features::debug::kAtMemorySkipEnablementChecks)
          ? (prefService &&
             prefService->GetBoolean(
                 personal_context::prefs::
                     kPersonalContextInAutofillSettingsToggleStatus))
          : autofill::IsAutofillAtMemorySearchUIEnabled(autofillClient);

  if (!isEnabled) {
    id<AtMemoryCommands> handler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), AtMemoryCommands);
    [handler dismissAtMemory];
  }
}

@end
