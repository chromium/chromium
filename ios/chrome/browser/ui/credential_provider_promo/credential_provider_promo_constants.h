// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_CONSTANTS_H_

namespace credential_provider_promo {

enum PromoSource {
  kPasswordCopied,
  kPasswordSaved,
  kRemindLaterSelected,
  kAutofillUsed,
};

enum PromoContext {
  kFirstStep,
  kLearnMore,
};
}  // namespace credential_provider_promo

#endif  // IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_CONSTANTS_H_
