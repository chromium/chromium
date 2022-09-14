// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/tailored_security/tailored_security_service_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// TODO(crbug.com/1358259): Replace test strings.
const std::u16string test_consent_string = u"test_consent_string";
const std::u16string test_unconsent_string = u"test_unconsent_string";

}  // namespace

namespace safe_browsing {
TailoredSecurityServiceInfobarDelegate::TailoredSecurityServiceInfobarDelegate(
    bool consent_status)
    : consent_status_(consent_status) {}

TailoredSecurityServiceInfobarDelegate*
TailoredSecurityServiceInfobarDelegate::FromInfobarDelegate(
    infobars::InfoBarDelegate* delegate) {
  return delegate->GetIdentifier() == TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE
             ? static_cast<TailoredSecurityServiceInfobarDelegate*>(delegate)
             : nullptr;
}

std::u16string TailoredSecurityServiceInfobarDelegate::GetMessageText() const {
  return consent_status_ ? test_consent_string : test_unconsent_string;
}

std::u16string TailoredSecurityServiceInfobarDelegate::GetDescription() const {
  return consent_status_ ? test_consent_string : nil;
}

std::u16string TailoredSecurityServiceInfobarDelegate::GetMessageActionText()
    const {
  return consent_status_ ? test_consent_string : test_unconsent_string;
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
