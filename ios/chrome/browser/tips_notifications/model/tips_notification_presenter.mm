// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_presenter.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_presenter.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_commands.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
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
