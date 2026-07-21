// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_FEATURES_H_
#define GOOGLE_APIS_GAIA_GAIA_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace gaia::features {

COMPONENT_EXPORT(GOOGLE_APIS)
BASE_DECLARE_FEATURE(kGetAccountCapabilitiesUsesGetAllVisibleUrl);

// Enables appending Google account session index (/u/[index]/) to the passkey
// unlock URL.
COMPONENT_EXPORT(GOOGLE_APIS)
BASE_DECLARE_FEATURE(kSigninChromePasskeyUnlockUrlUsesAccountIndex);

// Enables appending Google account session index (/u/[index]/) to the sync keys
// retrieval and recoverability degraded URLs.
COMPONENT_EXPORT(GOOGLE_APIS)
BASE_DECLARE_FEATURE(kSigninChromeSyncKeysUrlUsesAccountIndex);

// When enabled, IssueToken fetches return transient failure instead of a
// permanent one when receiving an HTTP 200 response with an unexpected body.
COMPONENT_EXPORT(GOOGLE_APIS)
BASE_DECLARE_FEATURE(kOAuth2MintTokenUnexpectedResponseBodyIsTransient);

}  // namespace gaia::features

#endif  // GOOGLE_APIS_GAIA_GAIA_FEATURES_H_
