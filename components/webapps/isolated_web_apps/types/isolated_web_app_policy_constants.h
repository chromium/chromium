// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_ISOLATED_WEB_APP_POLICY_CONSTANTS_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_ISOLATED_WEB_APP_POLICY_CONSTANTS_H_

#include "base/component_export.h"

namespace web_app {

// Keys for the IsolatedWebAppInstallForceList preference.
COMPONENT_EXPORT(ISOLATED_WEB_APPS) extern const char kPolicyUpdateManifestUrlKey[];
COMPONENT_EXPORT(ISOLATED_WEB_APPS) extern const char kPolicyWebBundleIdKey[];
COMPONENT_EXPORT(ISOLATED_WEB_APPS) extern const char kPolicyUpdateChannelKey[];
COMPONENT_EXPORT(ISOLATED_WEB_APPS) extern const char kPolicyAllowDowngradesKey[];
COMPONENT_EXPORT(ISOLATED_WEB_APPS) extern const char kPolicyPinnedVersionKey[];

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_ISOLATED_WEB_APP_POLICY_CONSTANTS_H_
