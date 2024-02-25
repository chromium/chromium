// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_CRED_MAN_SUPPORT_H_
#define COMPONENTS_WEBAUTHN_ANDROID_CRED_MAN_SUPPORT_H_

namespace webauthn {

// Enum for CredMan support levels.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webauthn
enum CredManSupport {
  // Indicates the the level of CredMan support hasn't been determined yet.
  // This must be zero because that's the default value of a `static int`.
  NOT_EVALUATED = 0,
  // Indicates that CredMan cannot be used.
  DISABLED = -1,
  // Indicates that CredMan can be used if the particular request demands it.
  IF_REQUIRED = 1,
  // Indicates that CredMan should be used, unless the request is
  // incompatible. (E.g. if the
  // request is for a non-discoverable credential in Play Services.)
  FULL_UNLESS_INAPPLICABLE = 2,
  // Indicates that CredMan should be used in parallel with Fido2Api.
  PARALLEL_WITH_FIDO_2 = 3,
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_CRED_MAN_SUPPORT_H_
