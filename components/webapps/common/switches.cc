// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/common/switches.h"

namespace webapps {
namespace switches {

// This flag causes the user engagement checks for showing app banners to be
// bypassed. It is intended to be used by developers who wish to test that their
// sites otherwise meet the criteria needed to show app banners.
const char kBypassAppBannerEngagementChecks[] =
    "bypass-app-banner-engagement-checks";

}  // namespace switches
}  // namespace webapps
