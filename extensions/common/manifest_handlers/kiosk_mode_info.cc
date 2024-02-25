// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;

using api::extensions_manifest_types::KioskSecondaryAppsType;

namespace {

// Whether "enabled_on_launch" manifest property for the extension should be
// respected or not. If false, secondary apps that specify this property will
// be ignored.
bool AllowSecondaryAppEnabledOnLaunch(const Extension* extension) {
  if (!extension)
    return false;

  const Feature* feature = FeatureProvider::GetBehaviorFeatures()->GetFeature(
      behavior_feature::kAllowSecondaryKioskAppEnabledOnLaunch);
  if (!feature)
    return false;

  return feature->IsAvailableToExtension(extension).is_available();
}

}  // namespace

SecondaryKioskAppInfo::SecondaryKioskAppInfo(
    const extensions::ExtensionId& id,
    const std::optional<bool>& enabled_on_launch)
    : id(id), enabled_on_launch(enabled_on_launch) {}

SecondaryKioskAppInfo::SecondaryKioskAppInfo(
    const SecondaryKioskAppInfo& other) = default;

SecondaryKioskAppInfo::~SecondaryKioskAppInfo() = default;

KioskModeInfo::KioskModeInfo(
    KioskStatus kiosk_status,
    std::vector<SecondaryKioskAppInfo>&& secondary_apps,
    const std::string& required_platform_version,
    bool always_update)
    : kiosk_status(kiosk_status),
      secondary_apps(std::move(secondary_apps)),
      required_platform_version(required_platform_version),
      always_update(always_update) {}

KioskModeInfo::~KioskModeInfo() {
}

// static
KioskModeInfo* KioskModeInfo::Get(const Extension* extension) {
  return static_cast<KioskModeInfo*>(
      extension->GetManifestData(keys::kKioskMode));
}

// static
bool KioskModeInfo::IsKioskEnabled(const Extension* extension) {
  KioskModeInfo* info = Get(extension);
  return info ? info->kiosk_status != NONE : false;
}

// static
bool KioskModeInfo::IsKioskOnly(const Extension* extension) {
  KioskModeInfo* info = Get(extension);
  return info ? info->kiosk_status == ONLY : false;
}

// static
bool KioskModeInfo::HasSecondaryApps(const Extension* extension) {
  KioskModeInfo* info = Get(extension);
  return info && !info->secondary_apps.empty();
}

// static
bool KioskModeInfo::IsValidPlatformVersion(const std::string& version_string) {
  const base::Version version(version_string);
  return version.IsValid() && version.components().size() <= 3u;
}

KioskModeHandler::KioskModeHandler() {
}

KioskModeHandler::~KioskModeHandler() {
}

bool KioskModeHandler::Parse(Extension* extension, std::u16string* error) {
  const Manifest* manifest = extension->manifest();
  DCHECK(manifest->FindKey(keys::kKioskEnabled) ||
         manifest->FindKey(keys::kKioskOnly));

  bool kiosk_enabled = false;
  if (const base::Value* temp = manifest->FindKey(keys::kKioskEnabled)) {
    if (!temp->is_bool()) {
      *error = manifest_errors::kInvalidKioskEnabled;
      return false;
    }
    kiosk_enabled = temp->GetBool();
  }

  bool kiosk_only = false;
  if (const base::Value* temp = manifest->FindKey(keys::kKioskOnly)) {
    if (!temp->is_bool()) {
      *error = manifest_errors::kInvalidKioskOnly;
      return false;
    }
    kiosk_only = temp->GetBool();
  }

  if (kiosk_only && !kiosk_enabled) {
    *error = manifest_errors::kInvalidKioskOnlyButNotEnabled;
    return false;
  }

  KioskModeInfo::KioskStatus kiosk_status = KioskModeInfo::NONE;
  if (kiosk_enabled)
    kiosk_status = kiosk_only ? KioskModeInfo::ONLY : KioskModeInfo::ENABLED;

  // Kiosk secondary apps key is optional.
  std::vector<SecondaryKioskAppInfo> secondary_apps;
  std::set<std::string> secondary_app_ids;
  if (manifest->FindKey(keys::kKioskSecondaryApps)) {
    const base::Value* secondary_apps_value = nullptr;
    if (!manifest->GetList(keys::kKioskSecondaryApps, &secondary_apps_value)) {
      *error = manifest_errors::kInvalidKioskSecondaryApps;
      return false;
    }

    const bool allow_enabled_on_launch =
        AllowSecondaryAppEnabledOnLaunch(extension);

    for (const auto& value : secondary_apps_value->GetList()) {
      auto app = KioskSecondaryAppsType::FromValue(value);
      if (!app.has_value()) {
        *error = manifest_errors::kInvalidKioskSecondaryAppsBadAppEntry;
        return false;
      }

      if (secondary_app_ids.count(app->id)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            manifest_errors::kInvalidKioskSecondaryAppsDuplicateApp, app->id);
        return false;
      }

      if (app->enabled_on_launch && !allow_enabled_on_launch) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            manifest_errors::kInvalidKioskSecondaryAppsPropertyUnavailable,
            "enabled_on_launch", app->id);
        return false;
      }

      std::optional<bool> enabled_on_launch;
      if (app->enabled_on_launch)
        enabled_on_launch = *app->enabled_on_launch;

      secondary_apps.emplace_back(app->id, enabled_on_launch);
      secondary_app_ids.insert(app->id);
    }
  }

  // Optional kiosk.required_platform_version key.
  std::string required_platform_version;
  if (const base::Value* temp =
          manifest->FindPath(keys::kKioskRequiredPlatformVersion)) {
    if (!temp->is_string() ||
        !KioskModeInfo::IsValidPlatformVersion(temp->GetString())) {
      *error = manifest_errors::kInvalidKioskRequiredPlatformVersion;
      return false;
    }
    required_platform_version = temp->GetString();
  }

  // Optional kiosk.always_update key.
  bool always_update = false;
  if (const base::Value* temp = manifest->FindPath(keys::kKioskAlwaysUpdate)) {
    if (!temp->is_bool()) {
      *error = manifest_errors::kInvalidKioskAlwaysUpdate;
      return false;
    }
    always_update = temp->GetBool();
  }

  extension->SetManifestData(keys::kKioskMode,
                             std::make_unique<KioskModeInfo>(
                                 kiosk_status, std::move(secondary_apps),
                                 required_platform_version, always_update));

  return true;
}

base::span<const char* const> KioskModeHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kKiosk, keys::kKioskEnabled,
                                          keys::kKioskOnly,
                                          keys::kKioskSecondaryApps};
  return kKeys;
}

}  // namespace extensions
