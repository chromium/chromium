// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_STUB_DIGITAL_IDENTITY_PROVIDER_H_
#define CONTENT_BROWSER_WEBID_TEST_STUB_DIGITAL_IDENTITY_PROVIDER_H_

#include "content/public/browser/digital_identity_provider.h"

namespace content {

class StubDigitalIdentityProvider : public DigitalIdentityProvider {
 public:
  StubDigitalIdentityProvider();
  ~StubDigitalIdentityProvider() override;

  StubDigitalIdentityProvider(const StubDigitalIdentityProvider&) = delete;
  StubDigitalIdentityProvider& operator=(const StubDigitalIdentityProvider&) =
      delete;

  bool IsLowRiskOrigin(const url::Origin& to_check) const override;
  DigitalIdentityInterstitialAbortCallback ShowDigitalIdentityInterstitial(
      WebContents& web_contents,
      const url::Origin& origin,
      DigitalIdentityInterstitialType interstitial_type,
      DigitalIdentityInterstitialCallback callback) override;
  void Request(WebContents*,
               const url::Origin& origin,
               base::Value request,
               DigitalIdentityCallback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_STUB_DIGITAL_IDENTITY_PROVIDER_H_
