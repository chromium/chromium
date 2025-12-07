// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_presenter.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_presenter.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_commands.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tips_notifications/model/tips_notification_criteria.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"

TipsNotificationPresenter::TipsNotificationPresenter(Browser* browser)
    : browser_(browser) {}

void TipsNotificationPresenter::Present(base::WeakPtr<Browser> weakBrowser,
                                        TipsNotificationType type) {
  Browser* browser = weakBrowser.get();
  if (!browser) {
    return;
  }
  id<ApplicationCommands> application_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  auto show_ui_callback = base::CallbackToBlock(base::BindOnce(
      &TipsNotificationPresenter::PresentInternal, weakBrowser, type));
  [application_handler
      prepareToPresentModalWithSnackbarDismissal:NO
                                      completion:show_ui_callback];
}

void TipsNotificationPresenter::PresentInternal(
    base::WeakPtr<Browser> weakBrowser,
    TipsNotificationType type) {
  Browser* browser = weakBrowser.get();
  if (!browser) {
    return;
  }
  TipsNotificationPresenter presenter(browser);
  presenter.Present(type);
}

void TipsNotificationPresenter::Present(TipsNotificationType type) {
  switch (type) {
    case TipsNotificationType::kDefaultBrowser:
      ShowDefaultBrowserPromo();
      break;
    case TipsNotificationType::kWhatsNew:
      ShowWhatsNew();
      break;
    case TipsNotificationType::kSignin:
      ShowSignin();
      break;
    case TipsNotificationType::kSetUpListContinuation:
      ShowSetUpListContinuation();
      break;
    case TipsNotificationType::kDocking:
      ShowDocking();
      break;
    case TipsNotificationType::kOmniboxPosition:
      ShowOmniboxPosition();
      break;
    case TipsNotificationType::kLens:
      ShowLensPromo();
      break;
    case TipsNotificationType::kEnhancedSafeBrowsing:
      ShowEnhancedSafeBrowsingPromo();
      break;
    case TipsNotificationType::kCPE:
      ShowCPEPromo();
      break;
    case TipsNotificationType::kLensOverlay:
      ShowLensOverlayPromo();
      break;
    case TipsNotificationType::kTrustedVaultKeyRetrieval:
      StartTrustedVaultKeyRetrievalFlow();
      break;
    case TipsNotificationType::kIncognitoLock:
    case TipsNotificationType::kError:
      NOTREACHED();
  }
}

void TipsNotificationPresenter::ShowDefaultBrowserPromo() {
  id<SettingsCommands> settings_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), SettingsCommands);
  [settings_handler
      showDefaultBrowserSettingsFromViewController:nil
                                      sourceForUMA:
                                          DefaultBrowserSettingsPageSource::
                                              kTipsNotification];
}

void TipsNotificationPresenter::ShowWhatsNew() {
  [HandlerForProtocol(browser_->GetCommandDispatcher(), WhatsNewCommands)
      showWhatsNew];
}

void TipsNotificationPresenter::ShowSignin() {
  // The user may have signed in between when the notification was requested
  // and when it triggered. If the user can no longer sign in, then open
  // the account settings.
  if (!TipsNotificationCriteria::CanSignIn(browser_->GetProfile())) {
    [HandlerForProtocol(browser_->GetCommandDispatcher(), SettingsCommands)
        showAccountsSettingsFromViewController:nil
                          skipIfUINotAvailable:NO];
    return;
  }
  // If there are 0 identities, kInstantSignin requires less taps.
  AuthenticationOperation operation =
      HasIdentitiesOnDevice() ? AuthenticationOperation::kSigninOnly
                              : AuthenticationOperation::kInstantSignin;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:operation
               identity:nil
            accessPoint:signin_metrics::AccessPoint::kTipsNotification
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
             completion:nil];

  [HandlerForProtocol(browser_->GetCommandDispatcher(), SigninPresenter)
      showSignin:command];
}

void TipsNotificationPresenter::ShowSetUpListContinuation() {
  [HandlerForProtocol(browser_->GetCommandDispatcher(),
                      ContentSuggestionsCommands)
      showSetUpListSeeMoreMenuExpanded:YES];
}

void TipsNotificationPresenter::ShowDocking() {
  [HandlerForProtocol(browser_->GetCommandDispatcher(), DockingPromoCommands)
      showDockingPromo:YES];
}

void TipsNotificationPresenter::ShowOmniboxPosition() {
  [HandlerForProtocol(browser_->GetCommandDispatcher(),
                      BrowserCoordinatorCommands) showOmniboxPositionChoice];
}

void TipsNotificationPresenter::ShowLensPromo() {
  [HandlerForProtocol(browser_->GetCommandDispatcher(),
                      BrowserCoordinatorCommands) showLensPromo];
}

void TipsNotificationPresenter::StartTrustedVaultKeyRetrievalFlow() {
  const char* metric_name =
      "IOS.PasswordManager.TrustedVaultNotification.Events";
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfileIfExists(browser_->GetProfile());
  if (sync_service == nullptr) {
    // Sync service might be not available for some profiles. If Sync service is
    // not available, we don't need to do anything.
    //
    // For example, this code branch might be activated in the
    // following scenario:
    // 1) The tips notification has been displayed for a user without a
    // trusted vault key.
    // 2) The user signs-out (and possibly signs-in to some other profile).
    // 3) Afterwards the user notices the tips notification and clicks "Open".
    // 4) Since the profile from the step 2 is not guaranteed to have a Sync
    // service, we need to check if sync_service exists.
    //
    // We expect that the described scenario is rare.
    base::UmaHistogramEnumeration(
        metric_name,
        TrustedVaultNotificationEvents::kSyncServiceDoesNotExistForProfile);
    return;
  }
  if (!sync_service->GetUserSettings()
           ->IsTrustedVaultKeyRequiredForPreferredDataTypes()) {
    // The trusted vault key is already available, so we don't need to do
    // anything.
    //
    // For example, this code branch might be activated in the
    // following scenario:
    // 1) The tips notification has been displayed for a user without a
    // trusted vault key.
    // 2) The user fixed the issue using some in-browser UI (e.g. via the
    // Password Manager settings UI).
    // 3) Afterwards the user notices the tips notification and clicks "Open".
    // 4) Since the key has been retrieved in the step 2 we don't need to
    // perform the key retrieval anymore.
    //
    // We expect that the described scenario is rare.
    base::UmaHistogramEnumeration(
        metric_name,
        TrustedVaultNotificationEvents::kTrustedVaultKeyAlreadyAvailable);
    return;
  }
  [HandlerForProtocol(browser_->GetCommandDispatcher(),
                      BrowserCoordinatorCommands)
      performReauthToRetrieveTrustedVaultKey:
          trusted_vault::TrustedVaultUserActionTriggerForUMA::kNotification];
  base::UmaHistogramEnumeration(
      metric_name, TrustedVaultNotificationEvents::kKeyRetrievalFlowStarted);
}

void TipsNotificationPresenter::ShowEnhancedSafeBrowsingPromo() {
  [HandlerForProtocol(browser_->GetCommandDispatcher(),
                      BrowserCoordinatorCommands)
      showEnhancedSafeBrowsingPromo];
}

void TipsNotificationPresenter::ShowCPEPromo() {
  [HandlerForProtocol(browser_->GetCommandDispatcher(),
                      CredentialProviderPromoCommands)
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 TipsNotification];
}

void TipsNotificationPresenter::ShowLensOverlayPromo() {
  [HandlerForProtocol(browser_->GetCommandDispatcher(),
                      BrowserCoordinatorCommands) showSearchWhatYouSeePromo];
}

bool TipsNotificationPresenter::HasIdentitiesOnDevice() {
  return !IdentityManagerFactory::GetForProfile(browser_->GetProfile())
              ->GetAccountsOnDevice()
              .empty();
}
