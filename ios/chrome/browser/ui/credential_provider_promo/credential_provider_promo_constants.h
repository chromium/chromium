// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_CONSTANTS_H_

enum CredentialProviderPromoSource {
  kPasswordCopied,
  kPasswordSaved,
  kRemindLaterSelected,
  kAutofillUsed,
};

enum CredentialProviderPromoContext {
  kFirstStep,
  kLearnMore,
};

#endif  // IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_CONSTANTS_H_
