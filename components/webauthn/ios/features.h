// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_FEATURES_H_
#define COMPONENTS_WEBAUTHN_IOS_FEATURES_H_

#include "base/feature_list.h"

// Shim the WebKit implementation of navigator.credentials to enable enhanced
// passkey-related features.
// Keep as a kill switch after enabling by default.
BASE_DECLARE_FEATURE(kIOSPasskeyShim);

// Allow modal passkey logins to happen directly in the browser, without using
// the Credential Provider Extension.
// This is a no-op if kIOSPasskeyShim is disabled.
BASE_DECLARE_FEATURE(kIOSPasskeyModalLoginWithShim);

#endif  // COMPONENTS_WEBAUTHN_IOS_FEATURES_H_
