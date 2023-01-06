// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_FEATURE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_FEATURE_H_

#include "base/feature_list.h"

namespace security_interstitials {
namespace features {

// Enables HTTPS-Only mode upgrades on iOS.
BASE_DECLARE_FEATURE(kHttpsOnlyMode);

// Enables mixed content autoupgrades on iOS.
BASE_DECLARE_FEATURE(kMixedContentAutoupgrade);

}  // namespace features
}  // namespace security_interstitials

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_FEATURE_H_
