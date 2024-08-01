// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_MODE_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_MODE_H_

namespace webauthn {

// Enum for WebAuthn mode.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webauthn
enum WebauthnMode {
  // WebAuthn is disabled.
  NONE = 0,
  // WebAuthn is enabled for app requests. Origin for the request cannot be sent
  // in this mode.
  APP = 1,
  // WebAuthn is enabled in browser mode. Requests for any origin can be made.
  BROWSER = 2,
  // WebAuthn is enabled for Chrome. It is a special browser mode for Chrome.
  CHROME = 3,
  // WebAuthn is enabled for Chrome using 3rd Party Password Managers. Unlike
  // CHROME mode, this mode does not support conditional requests pre
  // Android 14.
  CHROME_3PP_ENABLED = 4,
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_MODE_H_
