// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_FEATURES_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_FEATURES_H_

#include "base/features.h"

namespace features {

// Enables preventing installation and update of non-allowlisted IWAs.
BASE_DECLARE_FEATURE(kIsolatedWebAppManagedAllowlist);

}  // namespace features

namespace web_app {

// Enables the key distribution dev mode UI on chrome://web-app-internals.
BASE_DECLARE_FEATURE(kIwaKeyDistributionDevMode);

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_FEATURES_H_
