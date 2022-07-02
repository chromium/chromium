// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_FEATURE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_FEATURE_H_

#include "base/feature_list.h"

namespace security_interstitials {
namespace features {

// Enables the HTTPS-Only mode UI setting on iOS. The user has to enable
// the UI setting under "Privacy and Security" to use HTTPS-Only Mode.
extern const base::Feature kHttpsOnlyMode;

}  // namespace features
}  // namespace security_interstitials

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_FEATURE_H_
