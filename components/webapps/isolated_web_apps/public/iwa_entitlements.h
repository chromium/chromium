// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_PUBLIC_IWA_ENTITLEMENTS_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_PUBLIC_IWA_ENTITLEMENTS_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/values.h"
#include "components/webapps/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"

namespace web_app {

using IwaEntitlement =
    IwaAccessControl::UserInstallAllowlistItemData::Entitlement;
using IwaVersionRange =
    IwaAccessControl::UserInstallAllowlistItemData::VersionRange;

struct COMPONENT_EXPORT(ISOLATED_WEB_APPS) IwaEntitlementsSet {
  IwaEntitlementsSet();
  ~IwaEntitlementsSet();
  IwaEntitlementsSet(const IwaEntitlementsSet&);
  IwaEntitlementsSet& operator=(const IwaEntitlementsSet&);

  IwaVersionRange version_range;
  std::vector<IwaEntitlement> entitlements;

  base::Value AsDebugValue() const;
  bool operator==(const IwaEntitlementsSet&) const;
};

// Maps a permissions policy feature to the required entitlement for
// user-installed Isolated Web Apps. If the feature is not restricted by
// entitlements, returns std::nullopt.
//
// If the feature is `std::nullopt` it doesn't mean that the feature is allowed
// by default. It just means that it's not controlled by a specific entitlement.
// If it's `std::nullopt` and
// `network::IsPermissionsPolicyFeatureGuardedByIsolatedContext` returns true,
// then the feature is disallowed for user-installed IWAs.
COMPONENT_EXPORT(ISOLATED_WEB_APPS)
std::optional<IwaEntitlement> GetEntitlementForFeature(
    const std::string& feature_name);

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_PUBLIC_IWA_ENTITLEMENTS_H_
