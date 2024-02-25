// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_TYPES_H_
#define CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_TYPES_H_

namespace content::digital_identity {

// Do not reorder or change the values because the enum values are being
// recorded in metrics.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser.webid
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: DigitalIdentityRequestStatusForMetrics
enum class RequestStatusForMetrics {
  kSuccess = 0,
  kErrorOther = 1,
  kErrorNoCredential = 2,
  kErrorUserDeclined = 3,
  kErrorAborted = 4,
  kMaxValue = kErrorAborted,
};

}  // namespace content::digital_identity

#endif  // CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_TYPES_H_
