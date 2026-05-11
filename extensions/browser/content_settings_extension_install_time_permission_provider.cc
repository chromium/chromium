// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_settings_extension_install_time_permission_provider.h"

#include <memory>
#include <optional>

#include "base/containers/fixed_flat_map.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

namespace extensions {
namespace {

static constexpr auto kTypesToPermissions =
    base::MakeFixedFlatMap<ContentSettingsType,
                           extensions::mojom::APIPermissionID>({
        {ContentSettingsType::GEOLOCATION,
         extensions::mojom::APIPermissionID::kGeolocation},
        {ContentSettingsType::NOTIFICATIONS,
         extensions::mojom::APIPermissionID::kNotifications},
    });
}

ExtensionInstallTimePermissionProvider::ExtensionInstallTimePermissionProvider(
    content::BrowserContext* context,
    extensions::ExtensionRegistry* extension_registry)
    : extension_registry_(extension_registry) {
  extension_registry_->AddObserver(this);
  for (const scoped_refptr<const Extension>& extension :
       extension_registry->enabled_extensions()) {
    OnExtensionLoaded(context, extension.get());
  }
}

ExtensionInstallTimePermissionProvider::
    ~ExtensionInstallTimePermissionProvider() = default;

std::unique_ptr<content_settings::RuleIterator>
ExtensionInstallTimePermissionProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool off_the_record) const {
  return value_map_.GetRuleIterator(content_type);
}

std::unique_ptr<content_settings::Rule>
ExtensionInstallTimePermissionProvider::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record) const {
  base::AutoLock auto_lock(value_map_.GetLock());
  return value_map_.GetRule(primary_url, secondary_url, content_type);
}

bool ExtensionInstallTimePermissionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const base::Value& value,
    const content_settings::ContentSettingConstraints& constraints) {
  // Not supported.
  return false;
}

void ExtensionInstallTimePermissionProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  // Not supported.
}

void ExtensionInstallTimePermissionProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  RemoveAllObservers();
  extension_registry_->RemoveObserver(this);
  extension_registry_ = nullptr;
  base::AutoLock auto_lock(value_map_.GetLock());
  value_map_.clear();
}

void ExtensionInstallTimePermissionProvider::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  base::AutoLock auto_lock(value_map_.GetLock());
  for (const auto& [content_type, permission] : kTypesToPermissions) {
    if (extension->permissions_data()->HasAPIPermission(permission)) {
      value_map_.SetValue(
          ContentSettingsPattern::FromURLNoWildcard(extension->url()),
          ContentSettingsPattern::Wildcard(), content_type,
          base::Value(CONTENT_SETTING_ALLOW), content_settings::RuleMetaData{});
    }
  }
}

void ExtensionInstallTimePermissionProvider::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  base::AutoLock auto_lock(value_map_.GetLock());
  for (const auto& [content_type, permission] : kTypesToPermissions) {
    value_map_.DeleteValue(
        ContentSettingsPattern::FromURLNoWildcard(extension->url()),
        ContentSettingsPattern::Wildcard(), content_type);
  }
}

}  // namespace extensions
