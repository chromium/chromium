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
#include "content/public/browser/render_frame_host.h"
#include "url/origin.h"

namespace content {

class WebContents;

// Coordinates between the web and native apps such that the latter can share
// vcs with the web API caller, or the website issues vcs to native apps. The
// functions are platform agnostic and implementations are expected to be
// different across platforms like desktop and mobile.
class CONTENT_EXPORT DigitalIdentityProvider {
 public:
  struct CONTENT_EXPORT DigitalCredential {
    DigitalCredential(std::string protocol, base::Value data);
    DigitalCredential(DigitalCredential&& other);
    DigitalCredential& operator=(DigitalCredential&& other);
    DigitalCredential(DigitalCredential& other) = delete;
    DigitalCredential& operator=(const DigitalCredential&) = delete;
    ~DigitalCredential();

    // Returns a deep copy of this credential.
    DigitalCredential Clone() const;

    std::string protocol;
    base::Value data;
  };
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
    kErrorNoRequests = 5,
    kErrorNoTransientUserActivation = 6,
    kErrorInvalidJson = 7,
    kMaxValue = kErrorInvalidJson,
  };

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser.webid
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: DigitalIdentityUiWarningLevel
  enum class UiWarningLevel {
    kHintNoIssues = 0,
    kHintCaution = 1,
  };

  virtual ~DigitalIdentityProvider();

  DigitalIdentityProvider(const DigitalIdentityProvider&) = delete;
  DigitalIdentityProvider& operator=(const DigitalIdentityProvider&) = delete;

  // Returns whether the last committed origin of `render_frame_host` is a known
  // low risk origin for digital credential requests.
  virtual bool IsLastCommittedOriginLowRisk(
      RenderFrameHost& render_frame_host) const = 0;

  using DigitalIdentityCallback = base::OnceCallback<void(
      base::expected<DigitalCredential, RequestStatusForMetrics>)>;

  // Coordinates the call to present a digital credential between the web and
  // native apps.
  virtual void Get(WebContents* web_contents,
                   const url::Origin& origin,
                   base::ValueView request,
                   UiWarningLevel warning_level,
                   DigitalIdentityCallback callback) = 0;

  // Coordinates the call to issue a digital credential between the web and
  // native apps.
  virtual void Create(WebContents* web_contents,
                      const url::Origin& origin,
                      base::ValueView request,
                      DigitalIdentityCallback callback) = 0;

 protected:
  DigitalIdentityProvider();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIGITAL_IDENTITY_PROVIDER_H_
