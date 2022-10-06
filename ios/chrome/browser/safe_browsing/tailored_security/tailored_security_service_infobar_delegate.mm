// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/tailored_security/tailored_security_service_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// TODO(crbug.com/1358259): Replace test strings.
const std::u16string test_consent_enabled_string =
    u"test_consent_enabled_string";
const std::u16string test_consent_disabled_string =
    u"test_consent_disabled_string";
const std::u16string test_unconsent_enabled_string =
    u"test_unconsent_enabled_string";

}  // namespace

namespace safe_browsing {
TailoredSecurityServiceInfobarDelegate::TailoredSecurityServiceInfobarDelegate(
    TailoredSecurityServiceMessageState message_state)
    : message_state_(message_state) {}

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
      return test_consent_enabled_string;
    case TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled:
      return test_consent_disabled_string;
    case TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled:
      return test_unconsent_enabled_string;
  }
}

std::u16string TailoredSecurityServiceInfobarDelegate::GetDescription() const {
  switch (message_state_) {
    case TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled:
      return test_consent_enabled_string;
    case TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled:
      return test_consent_disabled_string;
    case TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled:
      return nil;
  }
}

std::u16string TailoredSecurityServiceInfobarDelegate::GetMessageActionText()
    const {
  switch (message_state_) {
    case TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled:
      return test_consent_enabled_string;
    case TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled:
      return test_consent_disabled_string;
    case TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled:
      return test_unconsent_enabled_string;
  }
}

bool TailoredSecurityServiceInfobarDelegate::IsConsented() const {
  switch (message_state_) {
    case TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled:
    case TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled:
      return true;
    case TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled:
      return false;
  }
}

bool TailoredSecurityServiceInfobarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate->GetIdentifier() == GetIdentifier();
}

infobars::InfoBarDelegate::InfoBarIdentifier
TailoredSecurityServiceInfobarDelegate::GetIdentifier() const {
  return TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE;
}

}  // namespace safe_browsing
