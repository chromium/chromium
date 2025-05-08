// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/features.h"

namespace features {

BASE_FEATURE(kIsolatedWebAppManagedAllowlist,
             "IsolatedWebAppManagedAllowlist",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

namespace web_app {
BASE_FEATURE(kIwaKeyDistributionDevMode,
             "IwaKeyDistributionDevMode",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace web_app
