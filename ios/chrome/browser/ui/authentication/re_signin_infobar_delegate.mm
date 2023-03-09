// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "base/logging.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
bool ReSignInInfoBarDelegate::Create(ChromeBrowserState* browser_state,
                                     web::WebState* web_state,
                                     id<SigninPresenter> presenter) {
  DCHECK(web_state);
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  DCHECK(infobar_manager);

  std::unique_ptr<infobars::InfoBar> infobar =
      ReSignInInfoBarDelegate::CreateInfoBar(infobar_manager, browser_state,
                                             presenter);
  if (!infobar)
    return false;
  return !!infobar_manager->AddInfoBar(std::move(infobar));
}

// static
std::unique_ptr<infobars::InfoBar> ReSignInInfoBarDelegate::CreateInfoBar(
    infobars::InfoBarManager* infobar_manager,
    ChromeBrowserState* browser_state,
    id<SigninPresenter> presenter) {
  DCHECK(infobar_manager);
  std::unique_ptr<ReSignInInfoBarDelegate> delegate =
      ReSignInInfoBarDelegate::CreateInfoBarDelegate(browser_state, presenter);
  if (!delegate)
    return nullptr;
  return CreateConfirmInfoBar(std::move(delegate));
}

// static
std::unique_ptr<ReSignInInfoBarDelegate>
ReSignInInfoBarDelegate::CreateInfoBarDelegate(
    ChromeBrowserState* browser_state,
    id<SigninPresenter> presenter) {
  DCHECK(browser_state);
  // Do not ask user to sign in if current profile is incognito.
  if (browser_state->IsOffTheRecord())
    return nullptr;
  // Returns null if user does not need to be prompted to sign in again.
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  // Don't show the notification if sign-in is not supported.
  switch (authService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      return nullptr;
  }
  if (!authService->ShouldReauthPromptForSignInAndSync())
    return nullptr;
  // Returns null if user has already signed in via some other path.
  if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    authService->ResetReauthPromptForSignInAndSync();
    return nullptr;
  }
  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR);
  // User needs to be reminded to sign in again. Creates a new infobar delegate
  // and returns it.
  return std::make_unique<ReSignInInfoBarDelegate>(browser_state, presenter);
}

ReSignInInfoBarDelegate::ReSignInInfoBarDelegate(
    ChromeBrowserState* browser_state,
    id<SigninPresenter> presenter)
    : browser_state_(browser_state),
      icon_([UIImage imageNamed:@"infobar_warning"]),
      presenter_(presenter) {
  DCHECK(browser_state_);
  DCHECK(!browser_state_->IsOffTheRecord());
}

ReSignInInfoBarDelegate::~ReSignInInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
ReSignInInfoBarDelegate::GetIdentifier() const {
  return RE_SIGN_IN_INFOBAR_DELEGATE_IOS;
}

std::u16string ReSignInInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_IOS_SYNC_LOGIN_INFO_OUT_OF_DATE);
}

int ReSignInInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string ReSignInInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      IDS_IOS_SYNC_INFOBAR_SIGN_IN_SETTINGS_BUTTON_MOBILE);
}

ui::ImageModel ReSignInInfoBarDelegate::GetIcon() const {
  return ui::ImageModel::FromImage(icon_);
}

bool ReSignInInfoBarDelegate::Accept() {
  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperationReauthenticate
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_RESIGNIN_INFOBAR];
  [presenter_ showSignin:command];

  // Stop displaying the infobar once user interacted with it.
  AuthenticationServiceFactory::GetForBrowserState(browser_state_)
      ->ResetReauthPromptForSignInAndSync();
  return true;
}

void ReSignInInfoBarDelegate::InfoBarDismissed() {
  // Stop displaying the infobar once user interacted with it.
  AuthenticationServiceFactory::GetForBrowserState(browser_state_)
      ->ResetReauthPromptForSignInAndSync();
}
