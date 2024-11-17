// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/security_interstitials/https_only_mode/feature.h"

namespace security_interstitials {
namespace features {

BASE_FEATURE(kHttpsOnlyMode, "HttpsOnlyMode", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHttpsUpgrades,
             "HttpsUpgrades",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInsecureFormSubmissionInterstitial,
             "InsecureFormSubmissionInterstitial",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
}  // namespace security_interstitials
