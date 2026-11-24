// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_coordinator.h"

#import "base/check.h"
#import "base/i18n/language_tag.h"
#import "base/memory/weak_ptr.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_mediator.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_container_view_controller.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_prompt_model.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/web/public/web_state.h"

@interface AutofillAISaveEntityCoordinator () <
    AutofillAISaveEntityContainerViewControllerDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation AutofillAISaveEntityCoordinator {
  // Autofill commands handler.
  __weak id<AutofillCommands> _autofillHandler;

  // Navigation controller that owns the save entity view controller.
  UINavigationController* _navigationController;

  // Mediator that handles the business logic of the save entity UI.
  AutofillAISaveEntityMediator* _mediator;

  // New and old entity, and the callback to notify Autofill AI.
  std::optional<autofill::SaveEntityParams> _params;

  // Weak reference to the origin WebState for pending prompt tracking.
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    web::WebState* activeWebState =
        browser->GetWebStateList()->GetActiveWebState();
    if (activeWebState) {
      _webState = activeWebState->GetWeakPtr();
    }
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(autofill::SaveEntityParams)params {
  self = [self initWithBaseViewController:viewController browser:browser];
  if (self) {
    _params = std::move(params);
  }
  return self;
}

- (void)start {
  ProfileIOS* profile = self.profile;
  autofill::EntityDataManager* dataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile);

  const base::i18n::LanguageTag appLocale =
      GetApplicationContext()->GetApplicationLocaleStorage()->GetTag();
  if (base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn)) {
    AutofillBottomSheetTabHelper* tabHelper =
        _webState ? AutofillBottomSheetTabHelper::FromWebState(_webState.get())
                  : nullptr;
    if (!tabHelper || !tabHelper->GetAutofillAiSaveEntityPromptModel()) {
      return;
    }
    _mediator =
        [[AutofillAISaveEntityMediator alloc] initWithTabHelper:tabHelper
                                              entityDataManager:dataManager
                                                      appLocale:appLocale];
  } else {
    CHECK(_params.has_value());
    _mediator =
        [[AutofillAISaveEntityMediator alloc] initWithParams:std::move(*_params)
                                           entityDataManager:dataManager
                                                   appLocale:appLocale];
    _params.reset();
  }

  _autofillHandler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                        AutofillCommands);
  CHECK(_autofillHandler);

  AutofillAISaveEntityContainerViewController* saveViewController =
      [[AutofillAISaveEntityContainerViewController alloc] init];
  saveViewController.mutator = _mediator;
  saveViewController.delegate = self;
  _mediator.consumer = saveViewController;
  _mediator.autofillHandler = _autofillHandler;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:saveViewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
  _navigationController.presentationController.delegate = self;
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  _navigationController.presentationController.delegate = nil;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [_mediator dismissSaving];
}

#pragma mark - AutofillAISaveEntityContainerViewControllerDelegate

- (void)didTapLinkWithURL:(CrURL*)url {
  // Ensure the link URL is a valid HTTP or HTTPS URL before opening in a new
  // tab.
  if (!url) {
    return;
  }
  const GURL& gurl = url.gurl;
  if (!gurl.is_valid() || !gurl.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  [_mediator didTapFooterLinkURL:gurl];

  if (!base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn)) {
    // Legacy trunk behavior: record an intentional decline strike.
    [_mediator dismissSaving];
  }
  // Dismiss the save entity UI before opening the new tab.
  // When the feature is enabled, the prompt remains pending in the tab helper
  // and will be restored when the user returns to this tab.
  [_autofillHandler dismissSaveEntityDialog];

  id<SceneCommands> sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler
      openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:gurl]];
}

@end
