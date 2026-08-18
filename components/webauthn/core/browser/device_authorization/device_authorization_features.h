// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_FEATURES_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_FEATURES_H_

#include "base/feature_list.h"

namespace webauthn::features {

// Controls whether device authorization keys are fetched from the server.
BASE_DECLARE_FEATURE(kFetchDeviceAuthorizationKeys);

}  // namespace webauthn::features

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_FEATURES_H_
