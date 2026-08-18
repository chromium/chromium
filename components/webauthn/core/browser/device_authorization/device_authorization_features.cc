// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/device_authorization/device_authorization_features.h"

#include "base/feature_list.h"

namespace webauthn::features {

BASE_FEATURE(kFetchDeviceAuthorizationKeys, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace webauthn::features
