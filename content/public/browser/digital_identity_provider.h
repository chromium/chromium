// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIGITAL_IDENTITY_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_DIGITAL_IDENTITY_PROVIDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "url/origin.h"

#include <string>

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
    kMaxValue = kErrorAborted,
  };

  virtual ~DigitalIdentityProvider();

  DigitalIdentityProvider(const DigitalIdentityProvider&) = delete;
  DigitalIdentityProvider& operator=(const DigitalIdentityProvider&) = delete;

  using DigitalIdentityCallback = base::OnceCallback<void(
      const std::string&,
      RequestStatusForMetrics status_for_metrics)>;
  virtual void Request(WebContents* web_contents,
                       const url::Origin& origin,
                       const std::string& request,
                       DigitalIdentityCallback callback) = 0;

 protected:
  DigitalIdentityProvider();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIGITAL_IDENTITY_PROVIDER_H_
