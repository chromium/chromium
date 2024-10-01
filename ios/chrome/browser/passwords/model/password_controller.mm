// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_controller.h"

#import <stddef.h>

#import <algorithm>
#import <map>
#import <memory>
#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/values.h"
#import "components/autofill/core/browser/ui/suggestion_type.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/core/common/password_form_generation_data.h"
#import "components/autofill/core/common/signatures.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/browser/password_bubble_experiment.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_generation_frame_helper.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/password_sync_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/password_controller_driver_helper.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/safe_browsing/core/browser/password_protection/password_reuse_detection_manager_client.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/model/notify_auto_signin_view_controller.h"
#import "ios/chrome/browser/passwords/model/password_controller_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_breach_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_protection_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_suggestion_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using autofill::FieldRendererId;
using autofill::FormActivityObserverBridge;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::PasswordFormGenerationData;
using base::SysNSStringToUTF16;
using base::SysUTF16ToNSString;
using base::SysUTF8ToNSString;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::PasswordForm;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordGenerationFrameHelper;
using password_manager::PasswordManager;
using password_manager::PasswordManagerClient;
using password_manager::metrics_util::PasswordDropdownState;
using safe_browsing::PasswordReuseDetectionManagerClient;
using web::WebState;

namespace {
// Types of password infobars to display.
enum class PasswordInfoBarType { SAVE, UPDATE };

// Duration for notify user auto-sign in dialog being displayed.
constexpr int kNotifyAutoSigninDuration = 3;  // seconds
}  // namespace

@interface PasswordController () <SharedPasswordControllerDelegate>

// View controller for auto sign-in notification, owned by this
// PasswordController.
@property(nonatomic, strong)
    NotifyUserAutoSigninViewController* notifyAutoSigninViewController;

// Displays infobar for `form` with `type`. If `type` is UPDATE, the user
// is prompted to update the password. If `type` is SAVE, the user is prompted
// to save the password.
- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManagerForUI>)form
               infoBarType:(PasswordInfoBarType)type
                    manual:(BOOL)manual;

// Removes infobar for given `type` if it exists. If it is not found the
// request is silently ignored (because that use case is expected).
- (void)removeInfoBarOfType:(PasswordInfoBarType)type manual:(BOOL)manual;

// Hides auto sign-in notification. Removes the view from superview and destroys
// the controller.
// TODO(crbug.com/40394758): Animate disappearance.
- (void)hideAutosigninNotification;

@end

@implementation PasswordController {
  std::unique_ptr<PasswordManager> _passwordManager;
  std::unique_ptr<PasswordManagerClient> _passwordManagerClient;
  std::unique_ptr<PasswordReuseDetectionManagerClient>
      _passwordReuseDetectionManagerClient;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  raw_ptr<WebState> _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Timer for hiding "Signing in as ..." notification.
  base::OneShotTimer _notifyAutoSigninTimer;

  // User credential waiting to be displayed in autosign-in snackbar, once tab
  // becomes active.
  std::unique_ptr<PasswordForm> _pendingAutoSigninPasswordForm;
}

- (instancetype)initWithWebState:(WebState*)webState {
  self = [self initWithWebState:webState
                         client:nullptr
           reuseDetectionClient:nullptr];
  return self;
}

- (instancetype)initWithWebState:(WebState*)webState
                          client:(std::unique_ptr<PasswordManagerClient>)
                                     passwordManagerClient
            reuseDetectionClient:
                (std::unique_ptr<PasswordReuseDetectionManagerClient>)
                    passwordReuseDetectionManagerClient {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    if (passwordManagerClient) {
      _passwordManagerClient = std::move(passwordManagerClient);
    } else {
      _passwordManagerClient.reset(new IOSChromePasswordManagerClient(self));
    }
    if (passwordReuseDetectionManagerClient) {
      _passwordReuseDetectionManagerClient =
          std::move(passwordReuseDetectionManagerClient);
    } else {
      _passwordReuseDetectionManagerClient.reset(
          new IOSChromePasswordReuseDetectionManagerClient(self));
    }
    _passwordManager.reset(new PasswordManager(_passwordManagerClient.get()));

    PasswordFormHelper* formHelper =
        [[PasswordFormHelper alloc] initWithWebState:webState];
    PasswordSuggestionHelper* suggestionHelper =
        [[PasswordSuggestionHelper alloc] initWithWebState:_webState];
    PasswordControllerDriverHelper* driverHelper =
        [[PasswordControllerDriverHelper alloc] initWithWebState:_webState];
    _sharedPasswordController = [[SharedPasswordController alloc]
        initWithWebState:_webState
                 manager:_passwordManager.get()
              formHelper:formHelper
        suggestionHelper:suggestionHelper
            driverHelper:driverHelper];
    _sharedPasswordController.delegate = self;
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - Properties

- (ukm::SourceId)ukmSourceId {
  return _webState ? ukm::GetSourceIdForWebStateDocument(_webState)
                   : ukm::kInvalidSourceId;
}

- (PasswordManagerClient*)passwordManagerClient {
  return _passwordManagerClient.get();
}

- (PasswordReuseDetectionManagerClient*)passwordReuseDetectionManagerClient {
  return _passwordReuseDetectionManagerClient.get();
}

#pragma mark - CRWWebStateObserver

// If Tab was shown, and there is a pending PasswordForm, display autosign-in
// notification.
- (void)webStateWasShown:(WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_pendingAutoSigninPasswordForm) {
    [self showAutosigninNotification:std::move(_pendingAutoSigninPasswordForm)];
    _pendingAutoSigninPasswordForm.reset();
  }
}

// If Tab was hidden, hide auto sign-in notification.
- (void)webStateWasHidden:(WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self hideAutosigninNotification];
}

- (void)webStateDestroyed:(WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
  _passwordManager.reset();
  _passwordManagerClient.reset();
  _passwordReuseDetectionManagerClient.reset();
}

#pragma mark - FormSuggestionProvider

- (id<FormSuggestionProvider>)suggestionProvider {
  return _sharedPasswordController;
}

#pragma mark - PasswordGenerationProvider

- (id<PasswordGenerationProvider>)generationProvider {
  return _sharedPasswordController;
}

#pragma mark - IOSChromePasswordManagerClientBridge

- (WebState*)webState {
  return _webState;
}

- (ProfileIOS*)profile {
  return _webState ? ProfileIOS::FromBrowserState(_webState->GetBrowserState())
                   : nullptr;
}

- (PasswordManager*)passwordManager {
  return _passwordManager.get();
}

- (const GURL&)lastCommittedURL {
  return _webState ? _webState->GetLastCommittedURL() : GURL::EmptyGURL();
}

- (void)showSavePasswordInfoBar:
            (std::unique_ptr<PasswordFormManagerForUI>)formToSave
                         manual:(BOOL)manual {
  [self showInfoBarForForm:std::move(formToSave)
               infoBarType:PasswordInfoBarType::SAVE
                    manual:manual];
}

- (void)showUpdatePasswordInfoBar:
            (std::unique_ptr<PasswordFormManagerForUI>)formToUpdate
                           manual:(BOOL)manual {
  [self showInfoBarForForm:std::move(formToUpdate)
               infoBarType:PasswordInfoBarType::UPDATE
                    manual:manual];
}

- (void)removePasswordInfoBarManualFallback:(BOOL)manual {
  [self removeInfoBarOfType:PasswordInfoBarType::SAVE manual:manual];
  [self removeInfoBarOfType:PasswordInfoBarType::UPDATE manual:manual];
}

// Shows auto sign-in notification and schedules hiding it after 3 seconds.
// TODO(crbug.com/40394758): Animate appearance.
- (void)showAutosigninNotification:(std::unique_ptr<PasswordForm>)formSignedIn {
  if (!_webState) {
    return;
  }

  // If a notification is already being displayed, hides the old one, then shows
  // the new one.
  if (self.notifyAutoSigninViewController) {
    _notifyAutoSigninTimer.Stop();
    [self hideAutosigninNotification];
  }

  // Creates view controller then shows the subview.
  self.notifyAutoSigninViewController =
      [[NotifyUserAutoSigninViewController alloc]
          initWithUsername:SysUTF16ToNSString(formSignedIn->username_value)
                   iconURL:formSignedIn->icon_url
          URLLoaderFactory:_webState->GetBrowserState()
                               ->GetSharedURLLoaderFactory()];
  if (![_delegate displaySignInNotification:self.notifyAutoSigninViewController
                                  fromTabId:_webState->GetStableIdentifier()]) {
    // The notification was not shown. Store the password form in
    // `_pendingAutoSigninPasswordForm` to show the notification later.
    _pendingAutoSigninPasswordForm = std::move(formSignedIn);
    self.notifyAutoSigninViewController = nil;
    return;
  }

  // Hides notification after 3 seconds.
  __weak PasswordController* weakSelf = self;
  _notifyAutoSigninTimer.Start(FROM_HERE,
                               base::Seconds(kNotifyAutoSigninDuration),
                               base::BindRepeating(^{
                                 [weakSelf hideAutosigninNotification];
                               }));
}

- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType
                                  URL:(const GURL&)URL
                             username:(const std::u16string&)username {
  [self.passwordBreachDispatcher showPasswordBreachForLeakType:leakType];
}

- (void)showPasswordProtectionWarning:(NSString*)warningText
                           completion:(void (^)(safe_browsing::WarningAction))
                                          completion {
  [self.passwordProtectionDispatcher showPasswordProtectionWarning:warningText
                                                        completion:completion];
}

- (void)showCredentialProviderPromo:(CredentialProviderPromoTrigger)trigger {
  [self.credentialProviderPromoHandler
      showCredentialProviderPromoWithTrigger:trigger];
}

#pragma mark - Private methods

// The dispatcher used for PasswordBreachCommands.
- (id<PasswordBreachCommands>)passwordBreachDispatcher {
  DCHECK(self.dispatcher);
  return HandlerForProtocol(self.dispatcher, PasswordBreachCommands);
}

// The dispatcher used for PasswordProtectionCommands.
- (id<PasswordProtectionCommands>)passwordProtectionDispatcher {
  DCHECK(self.dispatcher);
  return HandlerForProtocol(self.dispatcher, PasswordProtectionCommands);
}

// The handler used for CredentialProviderPromoCommands.
- (id<CredentialProviderPromoCommands>)credentialProviderPromoHandler {
  DCHECK(self.dispatcher);
  return HandlerForProtocol(self.dispatcher, CredentialProviderPromoCommands);
}

// The dispatcher used for PasswordSuggestionCommands.
- (id<PasswordSuggestionCommands>)passwordSuggestionDispatcher {
  DCHECK(self.dispatcher);
  return HandlerForProtocol(self.dispatcher, PasswordSuggestionCommands);
}

- (InfoBarIOS*)findInfobarOfType:(InfobarType)infobarType manual:(BOOL)manual {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);

  for (infobars::InfoBar* infobar : infoBarManager->infobars()) {
    InfoBarIOS* infoBarIOS = static_cast<InfoBarIOS*>(infobar);
    if (infoBarIOS->infobar_type() == infobarType &&
        infoBarIOS->skip_banner() == manual) {
      return infoBarIOS;
    }
  }

  return nil;
}

- (void)removeInfoBarOfType:(PasswordInfoBarType)type manual:(BOOL)manual {
  InfoBarIOS* infobar = nil;
  switch (type) {
    case PasswordInfoBarType::SAVE: {
      infobar = [self findInfobarOfType:InfobarType::kInfobarTypePasswordSave
                                 manual:manual];
      break;
    }
    case PasswordInfoBarType::UPDATE: {
      infobar = [self findInfobarOfType:InfobarType::kInfobarTypePasswordUpdate
                                 manual:manual];
      break;
    }
  }

  if (infobar) {
    InfoBarManagerImpl::FromWebState(_webState)->RemoveInfoBar(infobar);
  }
}

- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManagerForUI>)form
               infoBarType:(PasswordInfoBarType)type
                    manual:(BOOL)manual {
  if (!_webState) {
    return;
  }

  CHECK(self.profile);
  PrefService* prefs = self.profile->GetPrefs();
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.profile);
  const std::optional<std::string> accountToStorePassword =
      password_manager::sync_util::GetAccountForSaving(prefs, syncService);
  const password_manager::features_util::PasswordAccountStorageUserState
      accountStorageUserState = password_manager::features_util::
          ComputePasswordAccountStorageUserState(prefs, syncService);

  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);

  switch (type) {
    case PasswordInfoBarType::SAVE: {
      // Count only new infobar showings, not replacements.
      if (![self findInfobarOfType:InfobarType::kInfobarTypePasswordSave
                            manual:manual]) {
        base::UmaHistogramBoolean("PasswordManager.iOS.InfoBar.PasswordSave",
                                  true);
      }

      auto delegate = std::make_unique<IOSChromeSavePasswordInfoBarDelegate>(
          accountToStorePassword,
          /*password_update=*/false, accountStorageUserState, std::move(form),
          self.dispatcher);
      std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
          InfobarType::kInfobarTypePasswordSave, std::move(delegate),
          /*skip_banner=*/manual);
      infoBarManager->AddInfoBar(std::move(infobar),
                                 /*replace_existing=*/true);
      break;
    }
    case PasswordInfoBarType::UPDATE: {
      // Count only new infobar showings, not replacements.
      if (![self findInfobarOfType:InfobarType::kInfobarTypePasswordUpdate
                            manual:manual]) {
        base::UmaHistogramBoolean("PasswordManager.iOS.InfoBar.PasswordUpdate",
                                  true);
      }

      auto delegate = std::make_unique<IOSChromeSavePasswordInfoBarDelegate>(
          form->IsUpdateAffectingPasswordsStoredInTheGoogleAccount()
              ? accountToStorePassword
              : std::nullopt,
          /*password_update=*/true, accountStorageUserState, std::move(form),
          self.dispatcher);
      std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
          InfobarType::kInfobarTypePasswordUpdate, std::move(delegate),
          /*skip_banner=*/manual);
      infoBarManager->AddInfoBar(std::move(infobar),
                                 /*replace_existing=*/true);
      break;
    }
  }
}

- (void)hideAutosigninNotification {
  [self.notifyAutoSigninViewController willMoveToParentViewController:nil];
  [self.notifyAutoSigninViewController.view removeFromSuperview];
  [self.notifyAutoSigninViewController removeFromParentViewController];
  self.notifyAutoSigninViewController = nil;
}

#pragma mark - SharedPasswordControllerDelegate

- (void)sharedPasswordController:(SharedPasswordController*)controller
    showGeneratedPotentialPassword:(NSString*)generatedPotentialPassword
                         proactive:(BOOL)proactive
                   decisionHandler:(void (^)(BOOL accept))decisionHandler {
  [self.passwordSuggestionDispatcher
      showPasswordSuggestion:generatedPotentialPassword
                   proactive:proactive
                    webState:_webState
             decisionHandler:decisionHandler];
}

- (void)sharedPasswordController:(SharedPasswordController*)controller
             didAcceptSuggestion:(FormSuggestion*)suggestion {
  if (suggestion.type == autofill::SuggestionType::kAllSavedPasswordsEntry) {
    // Navigate to the settings list.
    [self.delegate displaySavedPasswordList];
  }
}

- (void)attachListenersForBottomSheet:
            (const std::vector<autofill::FieldRendererId>&)rendererIds
                           forFrameId:(const std::string&)frameId {
  AutofillBottomSheetTabHelper* bottomSheetTabHelper =
      AutofillBottomSheetTabHelper::FromWebState(_webState);
  if (bottomSheetTabHelper) {
    bottomSheetTabHelper->AttachPasswordListeners(rendererIds, frameId);
  }
}

- (void)attachListenersForPasswordGenerationBottomSheet:
            (const std::vector<autofill::FieldRendererId>&)rendererIds
                                             forFrameId:
                                                 (const std::string&)frameId {
  AutofillBottomSheetTabHelper* bottomSheetTabHelper =
      AutofillBottomSheetTabHelper::FromWebState(_webState);
  if (bottomSheetTabHelper) {
    bottomSheetTabHelper->AttachPasswordGenerationListeners(rendererIds,
                                                            frameId);
  }
}

- (void)detachListenersForBottomSheet:(const std::string&)frameId {
  AutofillBottomSheetTabHelper* bottomSheetTabHelper =
      AutofillBottomSheetTabHelper::FromWebState(_webState);
  if (bottomSheetTabHelper) {
    bottomSheetTabHelper->DetachPasswordListeners(frameId,
                                                  /*refocus = */ false);
  }
}

@end
