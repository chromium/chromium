// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_controller.h"

#import <stddef.h>

#import <algorithm>
#import <map>
#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/bind.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/values.h"
#import "components/autofill/core/browser/ui/popup_item_ids.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/core/common/password_form_generation_data.h"
#import "components/autofill/core/common/signatures.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/password_bubble_experiment.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_generation_frame_helper.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/password_controller_driver_helper.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/driver/sync_service.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/notify_auto_signin_view_controller.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/password_breach_commands.h"
#import "ios/chrome/browser/ui/commands/password_protection_commands.h"
#import "ios/chrome/browser/ui/commands/password_suggestion_commands.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
using password_manager::GetPageURLAndCheckTrustLevel;
using password_manager::PasswordForm;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordGenerationFrameHelper;
using password_manager::PasswordManager;
using password_manager::PasswordManagerClient;
using password_manager::metrics_util::LogPasswordDropdownShown;
using password_manager::metrics_util::PasswordDropdownState;
using web::WebFrame;
using web::WebState;

namespace {
// Types of password infobars to display.
enum class PasswordInfoBarType { SAVE, UPDATE };

// Duration for notify user auto-sign in dialog being displayed.
constexpr int kNotifyAutoSigninDuration = 3;  // seconds
// Helper to check if password manager rebranding finch flag is enabled.
BOOL IsPasswordManagerBrandingUpdateEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kIOSEnablePasswordManagerBrandingUpdate);
}
}  // namespace

@interface PasswordController () <SharedPasswordControllerDelegate>

// View controller for auto sign-in notification, owned by this
// PasswordController.
@property(nonatomic, strong)
    NotifyUserAutoSigninViewController* notifyAutoSigninViewController;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Tracks current potential generated password until accepted or rejected.
@property(nonatomic, copy) NSString* generatedPotentialPassword;

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
// TODO(crbug.com/435048): Animate disappearance.
- (void)hideAutosigninNotification;

@end

@implementation PasswordController {
  std::unique_ptr<PasswordManager> _passwordManager;
  std::unique_ptr<PasswordManagerClient> _passwordManagerClient;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Timer for hiding "Signing in as ..." notification.
  base::OneShotTimer _notifyAutoSigninTimer;

  // User credential waiting to be displayed in autosign-in snackbar, once tab
  // becomes active.
  std::unique_ptr<PasswordForm> _pendingAutoSigninPasswordForm;
}

- (instancetype)initWithWebState:(WebState*)webState {
  self = [self initWithWebState:webState client:nullptr];
  return self;
}

- (instancetype)initWithWebState:(WebState*)webState
                          client:(std::unique_ptr<PasswordManagerClient>)
                                     passwordManagerClient {
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

- (ChromeBrowserState*)browserState {
  return _webState ? ChromeBrowserState::FromBrowserState(
                         _webState->GetBrowserState())
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
// TODO(crbug.com/435048): Animate appearance.
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

#pragma mark - Private methods

// Returns the user email.
- (NSString*)userEmail {
  DCHECK(self.browserState);

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(self.browserState);
  id<SystemIdentity> authenticatedIdentity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  return authenticatedIdentity.userEmail;
}

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

// The dispatcher used for PasswordSuggestionCommands.
- (id<PasswordSuggestionCommands>)passwordSuggestionDispatcher {
  DCHECK(self.dispatcher);
  return HandlerForProtocol(self.dispatcher, PasswordSuggestionCommands);
}

- (InfoBarIOS*)findInfobarOfType:(InfobarType)infobarType manual:(BOOL)manual {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);

  size_t count = infoBarManager->infobar_count();
  for (size_t i = 0; i < count; i++) {
    InfoBarIOS* infobar =
        static_cast<InfoBarIOS*>(infoBarManager->infobar_at(i));
    if (infobar->infobar_type() == infobarType &&
        infobar->skip_banner() == manual)
      return infobar;
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

  bool isSyncUser = false;
  if (self.browserState) {
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForBrowserState(self.browserState);
    isSyncUser =
        password_bubble_experiment::HasChosenToSyncPasswords(syncService);
  }
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(self.browserState);
  id<SystemIdentity> authenticatedIdentity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  switch (type) {
    case PasswordInfoBarType::SAVE: {
      // Count only new infobar showings, not replacements.
      if (![self findInfobarOfType:InfobarType::kInfobarTypePasswordSave
                            manual:manual]) {
        base::UmaHistogramBoolean("PasswordManager.iOS.InfoBar.PasswordSave",
                                  true);
      }

      auto delegate = std::make_unique<IOSChromeSavePasswordInfoBarDelegate>(
          authenticatedIdentity.userEmail, isSyncUser,
          /*password_update=*/false, std::move(form));
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
          base::UmaHistogramBoolean(
              "PasswordManager.iOS.InfoBar.PasswordUpdate", true);
        }

        auto delegate = std::make_unique<IOSChromeSavePasswordInfoBarDelegate>(
            authenticatedIdentity.userEmail, isSyncUser,
            /*password_update=*/true, std::move(form));
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

- (void)generatePasswordPopupDismissed {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  self.generatedPotentialPassword = nil;
}

- (void)updateGeneratePasswordStrings:(id)sender {
  NSString* title;
  NSString* message;

  if (IsPasswordManagerBrandingUpdateEnabled()) {
    title = [NSString
        stringWithFormat:@"%@\n%@\n ",
                         GetNSString(IDS_IOS_SUGGESTED_STRONG_PASSWORD),
                         self.generatedPotentialPassword];
    message = l10n_util::GetNSStringF(
        IDS_IOS_SUGGESTED_STRONG_PASSWORD_HINT_DISPLAYING_EMAIL,
        base::SysNSStringToUTF16([self userEmail]));
  } else {
    title = [NSString stringWithFormat:@"%@\n%@\n ",
                                       GetNSString(IDS_IOS_SUGGESTED_PASSWORD),
                                       self.generatedPotentialPassword];
    message = GetNSString(IDS_IOS_SUGGESTED_PASSWORD_HINT);
  }

  self.actionSheetCoordinator.attributedTitle =
      [[NSMutableAttributedString alloc]
          initWithString:title
              attributes:@{
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
              }];

  self.actionSheetCoordinator.attributedMessage =
      [[NSMutableAttributedString alloc]
          initWithString:message
              attributes:@{
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
              }];

  // TODO(crbug.com/886583): find a way to make action sheet coordinator
  // responsible for font size changes.
  [self.actionSheetCoordinator updateAttributedText];
}

#pragma mark - SharedPasswordControllerDelegate

- (void)sharedPasswordController:(SharedPasswordController*)controller
    showGeneratedPotentialPassword:(NSString*)generatedPotentialPassword
                   decisionHandler:(void (^)(BOOL accept))decisionHandler {
  self.generatedPotentialPassword = generatedPotentialPassword;

  if (IsPasswordManagerBrandingUpdateEnabled()) {
    [self.passwordSuggestionDispatcher
        showPasswordSuggestion:generatedPotentialPassword
               decisionHandler:decisionHandler];
  } else {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(updateGeneratePasswordStrings:)
               name:UIContentSizeCategoryDidChangeNotification
             object:nil];

    // TODO(crbug.com/886583): add eg tests
    self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:nullptr
                             title:@""
                           message:@""
                              rect:self.baseViewController.view.frame
                              view:self.baseViewController.view];
    self.actionSheetCoordinator.popoverArrowDirection = 0;
    self.actionSheetCoordinator.alertStyle =
        (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
            ? UIAlertControllerStyleAlert
            : UIAlertControllerStyleActionSheet;

    // Set attributed text.
    [self updateGeneratePasswordStrings:self];

    __weak PasswordController* weakSelf = self;

    auto popupDismissed = ^{
      [weakSelf generatePasswordPopupDismissed];
    };

    auto closeKeyboard = ^{
      if (!weakSelf.webState) {
        return;
      }
      FormInputAccessoryViewHandler* handler =
          [[FormInputAccessoryViewHandler alloc] init];
      NSString* mainFrameID =
          SysUTF8ToNSString(web::GetMainWebFrameId(weakSelf.webState));
      [handler setLastFocusFormActivityWebFrameID:mainFrameID];
      [handler closeKeyboardWithoutButtonPress];
    };

    NSString* primaryActionString;
    if (IsPasswordManagerBrandingUpdateEnabled()) {
      primaryActionString = GetNSString(IDS_IOS_USE_SUGGESTED_STRONG_PASSWORD);
    } else {
      primaryActionString = GetNSString(IDS_IOS_USE_SUGGESTED_PASSWORD);
    }

    [self.actionSheetCoordinator addItemWithTitle:primaryActionString
                                           action:^{
                                             decisionHandler(YES);
                                             popupDismissed();
                                             closeKeyboard();
                                           }
                                            style:UIAlertActionStyleDefault];

    [self.actionSheetCoordinator addItemWithTitle:GetNSString(IDS_CANCEL)
                                           action:^{
                                             decisionHandler(NO);
                                             popupDismissed();
                                           }
                                            style:UIAlertActionStyleCancel];

    // Set 'suggest' as preferred action, as per UX.
    self.actionSheetCoordinator.alertController.preferredAction =
        self.actionSheetCoordinator.alertController.actions[0];

    [self.actionSheetCoordinator start];
  }
}

- (void)sharedPasswordController:(SharedPasswordController*)controller
             didAcceptSuggestion:(FormSuggestion*)suggestion {
  if (suggestion.identifier ==
      autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY) {
    // Navigate to the settings list.
    [self.delegate displaySavedPasswordList];
  }
}

@end
