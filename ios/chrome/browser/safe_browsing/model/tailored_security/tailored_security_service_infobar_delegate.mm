// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_infobar_delegate.h"

#import "base/metrics/histogram_functions.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_outcome.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace safe_browsing {
TailoredSecurityServiceInfobarDelegate::TailoredSecurityServiceInfobarDelegate(
    TailoredSecurityServiceMessageState message_state,
    web::WebState* web_state)
    : message_state_(message_state),
      web_state_(web_state ? web_state->GetWeakPtr() : nullptr) {}

TailoredSecurityServiceInfobarDelegate::
    ~TailoredSecurityServiceInfobarDelegate() = default;

TailoredSecurityServiceInfobarDelegate*
TailoredSecurityServiceInfobarDelegate::FromInfobarDelegate(
    infobars::InfoBarDelegate* delegate) {
  return delegate->GetIdentifier() == TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE
             ? static_cast<TailoredSecurityServiceInfobarDelegate*>(delegate)
             : nullptr;
}

std::u16string TailoredSecurityServiceInfobarDelegate::GetMessageText() const {
  switch (message_state_) {
    case TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled:
      return l10n_util::GetStringUTF16(
          IDS_IOS_TAILORED_SECURITY_CONSENTED_ENABLE_MESSAGE_TITLE);
    case TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled:
      return l10n_util::GetStringUTF16(
          IDS_IOS_TAILORED_SECURITY_CONSENTED_DISABLE_MESSAGE_TITLE);
    case TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled:
      return l10n_util::GetStringUTF16(
          IDS_IOS_TAILORED_SECURITY_UNCONSENTED_ENABLE_MESSAGE_TITLE);
  }
}

std::u16string TailoredSecurityServiceInfobarDelegate::GetDescription() const {
  switch (message_state_) {
    case TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled:
      return l10n_util::GetStringUTF16(
          IDS_IOS_TAILORED_SECURITY_CONSENTED_ENABLE_MESSAGE_DESCRIPTION);
    case TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled:
      return l10n_util::GetStringUTF16(
          IDS_IOS_TAILORED_SECURITY_CONSENTED_DISABLE_MESSAGE_DESCRIPTION);
    case TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled:
      return l10n_util::GetStringUTF16(
          IDS_IOS_TAILORED_SECURITY_UNCONSENTED_ENABLE_MESSAGE_DESCRIPTION);
  }
}

std::u16string TailoredSecurityServiceInfobarDelegate::GetMessageActionText()
    const {
  switch (message_state_) {
    case TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled:
    case TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled:
      return l10n_util::GetStringUTF16(IDS_IOS_EDIT_ACTION_TITLE);
    case TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled:
      return l10n_util::GetStringUTF16(
          IDS_IOS_TAILORED_SECURITY_TURN_ON_ACTION_TITLE);
  }
}

bool TailoredSecurityServiceInfobarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate->GetIdentifier() == GetIdentifier();
}

bool TailoredSecurityServiceInfobarDelegate::Accept() {
  if (web_state_) {
    bool enabled =
        message_state_ ==
        TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled;
    std::string consentedHistogram =
        enabled
            ? "SafeBrowsing.TailoredSecurityConsentedEnabledMessageOutcome"
            : "SafeBrowsing.TailoredSecurityConsentedDisabledMessageOutcome";
    switch (message_state_) {
      case TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled:
      case TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled:
        base::UmaHistogramEnumeration(consentedHistogram,
                                      TailoredSecurityOutcome::kSettings);
        SafeBrowsingTabHelper::FromWebState(web_state_.get())
            ->OpenSafeBrowsingSettings();
        break;
      case TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled:
        base::UmaHistogramEnumeration(
            "SafeBrowsing.TailoredSecurityUnconsentedInFlowMessageOutcome",
            TailoredSecurityOutcome::kAccepted);
        ProfileIOS* profile =
            ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
        SetSafeBrowsingState(
            profile->GetPrefs(),
            safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION,
            /*is_esb_enabled_in_sync=*/false);
        break;
    }
  }
  return true;
}

infobars::InfoBarDelegate::InfoBarIdentifier
TailoredSecurityServiceInfobarDelegate::GetIdentifier() const {
  return TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE;
}

}  // namespace safe_browsing
