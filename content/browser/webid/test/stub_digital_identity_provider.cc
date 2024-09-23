// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/stub_digital_identity_provider.h"

#include "content/public/browser/digital_identity_provider.h"

namespace content {

using DigitalIdentityInterstitialAbortCallback =
    content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback;

StubDigitalIdentityProvider::StubDigitalIdentityProvider() = default;
StubDigitalIdentityProvider::~StubDigitalIdentityProvider() = default;

bool StubDigitalIdentityProvider::IsLowRiskOrigin(
    const url::Origin& to_check) const {
  return false;
}

DigitalIdentityInterstitialAbortCallback
StubDigitalIdentityProvider::ShowDigitalIdentityInterstitial(
    WebContents& web_contents,
    const url::Origin& origin,
    DigitalIdentityInterstitialType interstitial_type,
    DigitalIdentityInterstitialCallback callback) {
  return base::OnceClosure();
}

void StubDigitalIdentityProvider::Request(WebContents*,
                                          const url::Origin& origin,
                                          base::Value request,
                                          DigitalIdentityCallback) {}

}  // namespace content
