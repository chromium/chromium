// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "base/logging.h"
#import "base/memory/ptr_util.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"

// static
std::unique_ptr<ReSignInInfoBarDelegate> ReSignInInfoBarDelegate::Create(
    AuthenticationService* authentication_service,
    signin::IdentityManager* identity_manager,
    AppState* app_state,
    id<SigninPresenter> signin_presenter) {
  if (app_state.initStage != AppInitStage::kFinal) {
    return nullptr;
  }

  if (!authentication_service) {
    // Do not show the infobar on incognito.
    return nullptr;
  }

  switch (authentication_service->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      return nullptr;
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      break;
  }

  if (!authentication_service->ShouldReauthPromptForSignInAndSync()) {
    return nullptr;
  }

  if (authentication_service->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    authentication_service->ResetReauthPromptForSignInAndSync();
    return nullptr;
  }

  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR);
  return base::WrapUnique(new ReSignInInfoBarDelegate(
      authentication_service, identity_manager, signin_presenter));
}

ReSignInInfoBarDelegate::ReSignInInfoBarDelegate(
    AuthenticationService* authentication_service,
    signin::IdentityManager* identity_manager,
    id<SigninPresenter> signin_presenter)
    : authentication_service_(authentication_service),
      signin_presenter_(signin_presenter) {
  identity_manager_observer_.Observe(identity_manager);
  CHECK(authentication_service_);
  CHECK(signin_presenter_);
}

ReSignInInfoBarDelegate::~ReSignInInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
ReSignInInfoBarDelegate::GetIdentifier() const {
  return RE_SIGN_IN_INFOBAR_DELEGATE_IOS;
}

bool ReSignInInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

std::u16string ReSignInInfoBarDelegate::GetTitleText() const {
  return l10n_util::GetStringUTF16(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_ENCRYPTION_FIX_NOW);
}

std::u16string ReSignInInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_IOS_SYNC_LOGIN_INFO_OUT_OF_DATE_WITH_UNO);
}

int ReSignInInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string ReSignInInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      IDS_IOS_IDENTITY_ERROR_INFOBAR_VERIFY_BUTTON_LABEL);
}

ui::ImageModel ReSignInInfoBarDelegate::GetIcon() const {
  return ui::ImageModel::FromImage(gfx::Image(
      DefaultSymbolWithPointSize(kWarningFillSymbol, kInfobarSymbolPointSize)));
}

bool ReSignInInfoBarDelegate::Accept() {
  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kResignin
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_RESIGNIN_INFOBAR];
  [signin_presenter_ showSignin:command];

  // Stop displaying the infobar once user interacted with it.
  authentication_service_->ResetReauthPromptForSignInAndSync();
  identity_manager_observer_.Reset();
  return true;
}

void ReSignInInfoBarDelegate::InfoBarDismissed() {
  // Stop displaying the infobar once user interacted with it.
  authentication_service_->ResetReauthPromptForSignInAndSync();
  identity_manager_observer_.Reset();
}

void ReSignInInfoBarDelegate::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      identity_manager_observer_.Reset();
      infobar()->RemoveSelf();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}
