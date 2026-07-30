// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_ISOLATED_WEB_APP_POLICY_CONSTANTS_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_ISOLATED_WEB_APP_POLICY_CONSTANTS_H_

namespace web_app {

// Keys for the IsolatedWebAppInstallForceList preference.
extern const char kPolicyUpdateManifestUrlKey[];
extern const char kPolicyWebBundleIdKey[];
extern const char kPolicyUpdateChannelKey[];
extern const char kPolicyAllowDowngradesKey[];
extern const char kPolicyPinnedVersionKey[];

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_ISOLATED_WEB_APP_POLICY_CONSTANTS_H_
