// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIGITAL_IDENTITY_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_DIGITAL_IDENTITY_PROVIDER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/digital_identity_interstitial_type.h"
#include "url/origin.h"

namespace content {

class WebContents;

// Coordinates between the web and native apps such that the latter can share
// vcs with the web API caller. The functions are platform agnostic and
// implementations are expected to be different across platforms like desktop
// and mobile.
class CONTENT_EXPORT DigitalIdentityProvider {
 public:
  // Do not reorder or change the values because the enum values are being
  // recorded in metrics.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser.webid
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: DigitalIdentityRequestStatusForMetrics
  enum class RequestStatusForMetrics {
    kSuccess = 0,
    kErrorOther = 1,
    kErrorNoCredential = 2,
    kErrorUserDeclined = 3,
    kErrorAborted = 4,
    kErrorNoProviders = 5,
    kErrorNoTransientUserActivation = 6,
    kMaxValue = kErrorNoTransientUserActivation,
  };

  virtual ~DigitalIdentityProvider();

  DigitalIdentityProvider(const DigitalIdentityProvider&) = delete;
  DigitalIdentityProvider& operator=(const DigitalIdentityProvider&) = delete;

  // Returns whether the origin is a known low risk origin for which the
  // digital credential interstitial should not be shown regardless of the
  // credential being requested.
  virtual bool IsLowRiskOrigin(const url::Origin& to_check) const = 0;

  // Show interstitial to prompt user whether they want to share their identity
  // with the web page. Runs callback after the user dismisses the interstitial.
  // Returns a callback to call if the digital identity request is aborted. The
  // callback updates the interstitial UI to inform the user that the credential
  // request has been canceled. Returns an empty callback if no interstitial was
  // shown.
  using DigitalIdentityInterstitialAbortCallback = base::OnceClosure;
  using DigitalIdentityInterstitialCallback = base::OnceCallback<void(
      DigitalIdentityProvider::RequestStatusForMetrics status_for_metrics)>;
  virtual DigitalIdentityInterstitialAbortCallback
  ShowDigitalIdentityInterstitial(
      WebContents& web_contents,
      const url::Origin& origin,
      DigitalIdentityInterstitialType interstitial_type,
      DigitalIdentityInterstitialCallback callback) = 0;

  using DigitalIdentityCallback = base::OnceCallback<void(
      const base::expected<std::string, RequestStatusForMetrics>&)>;
  virtual void Request(WebContents* web_contents,
                       const url::Origin& origin,
                       base::Value request,
                       DigitalIdentityCallback callback) = 0;

 protected:
  DigitalIdentityProvider();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIGITAL_IDENTITY_PROVIDER_H_
