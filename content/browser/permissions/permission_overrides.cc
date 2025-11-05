// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_overrides.h"

#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/types/optional_ref.h"
#include "base/types/optional_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/permission_result.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

namespace content {
using PermissionStatus = blink::mojom::PermissionStatus;

PermissionOverrides::PermissionKey::PermissionKey(
    base::optional_ref<const url::Origin> requesting_origin,
    base::optional_ref<const url::Origin> embedding_origin,
    blink::PermissionType type)
    : scope_(MakeScopeData(requesting_origin, embedding_origin, type)),
      type_(type) {}

PermissionOverrides::PermissionKey::PermissionKey(blink::PermissionType type)
    : PermissionKey(std::nullopt, std::nullopt, type) {}

PermissionOverrides::PermissionKey::PermissionScope
PermissionOverrides::PermissionKey::MakeScopeData(
    base::optional_ref<const url::Origin> requesting_origin,
    base::optional_ref<const url::Origin> embedding_origin,
    blink::PermissionType type) {
  CHECK_EQ(requesting_origin.has_value(), embedding_origin.has_value());

  if (!requesting_origin.has_value()) {
    return PermissionOverrides::PermissionKey::GlobalKey();
  }

  // STORAGE_ACCESS_GRANT has a permission key of type (site, site) tuple as
  // defined by the spec:
  // https://privacycg.github.io/storage-access/#permissions-integration
  // TOP_LEVEL_STORAGE_ACCESS has a permission key of type (origin, site) tuple
  // as defined by the spec:
  // https://privacycg.github.io/requestStorageAccessFor/#permissions-integration
  switch (type) {
    case blink::PermissionType::STORAGE_ACCESS_GRANT:
      return std::make_pair(net::SchemefulSite(*requesting_origin),
                            net::SchemefulSite(*embedding_origin));
    case blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS:
      return std::make_pair(*requesting_origin,
                            net::SchemefulSite(*embedding_origin));
    default:
      // All other permission types use the top-level origin as the "permission
      // key". See
      // https://www.w3.org/TR/permissions/#dfn-default-permission-key-generation-algorithm.
      return *embedding_origin;
  }
}

PermissionOverrides::PermissionKey::PermissionKey() = default;
PermissionOverrides::PermissionKey::~PermissionKey() = default;
PermissionOverrides::PermissionKey::PermissionKey(const PermissionKey&) =
    default;
PermissionOverrides::PermissionKey&
PermissionOverrides::PermissionKey::operator=(const PermissionKey& other) =
    default;
PermissionOverrides::PermissionKey::PermissionKey(PermissionKey&&) = default;
PermissionOverrides::PermissionKey&
PermissionOverrides::PermissionKey::operator=(PermissionKey&& other) = default;

PermissionOverrides::PermissionOverrides() = default;
PermissionOverrides::~PermissionOverrides() = default;
PermissionOverrides::PermissionOverrides(PermissionOverrides&& other) = default;
PermissionOverrides& PermissionOverrides::operator=(
    PermissionOverrides&& other) = default;

void PermissionOverrides::Set(
    base::optional_ref<const url::Origin> requesting_origin,
    base::optional_ref<const url::Origin> embedding_origin,
    blink::PermissionType permission,
    const blink::mojom::PermissionStatus& status) {
  overrides_[PermissionKey(requesting_origin, embedding_origin, permission)] =
      status;

  // Special override status - MIDI_SYSEX is stronger than MIDI, meaning that
  // granting MIDI_SYSEX implies granting MIDI, while denying MIDI implies
  // denying MIDI_SYSEX.
  if (permission == blink::PermissionType::MIDI &&
      status != PermissionStatus::GRANTED) {
    overrides_[PermissionKey(requesting_origin, embedding_origin,
                             blink::PermissionType::MIDI_SYSEX)] = status;
  } else if (permission == blink::PermissionType::MIDI_SYSEX &&
             status == PermissionStatus::GRANTED) {
    overrides_[PermissionKey(requesting_origin, embedding_origin,
                             blink::PermissionType::MIDI)] = status;
  }
}

std::optional<PermissionResult> PermissionOverrides::Get(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    blink::PermissionType permission) const {
  const auto* status = base::FindOrNull(
      overrides_,
      PermissionKey(requesting_origin, embedding_origin, permission));
  if (!status) {
    status = base::FindOrNull(overrides_, PermissionKey(permission));
  }

  if (!status) {
    return std::nullopt;
  }

  // The Storage Access API spec calls for avoiding exposure of rejections to
  // prevent any attempt at retaliating against users who would reject a prompt:
  // https://privacycg.github.io/storage-access/#permissions-integration
  PermissionStatus adjusted_status = *status;
  if (permission == blink::PermissionType::STORAGE_ACCESS_GRANT &&
      *status == PermissionStatus::DENIED) {
    adjusted_status = PermissionStatus::ASK;
  }

  return PermissionResult(adjusted_status, PermissionStatusSource::UNSPECIFIED);
}

std::vector<ContentSettingPatternSource>
PermissionOverrides::CreateContentSettingsForType(
    blink::PermissionType permission_type) const {
  std::vector<ContentSettingPatternSource> patterns;
  for (const auto& [key, status] : overrides_) {
    if (permission_type != key.type()) {
      continue;
    }

    std::optional<ContentSetting> setting;
    switch (status) {
      case blink::mojom::PermissionStatus::GRANTED:
        setting = ContentSetting::CONTENT_SETTING_ALLOW;
        break;
      case blink::mojom::PermissionStatus::DENIED:
        setting = ContentSetting::CONTENT_SETTING_BLOCK;
        break;
      case blink::mojom::PermissionStatus::ASK:
        continue;
    }
    std::pair<ContentSettingsPattern, ContentSettingsPattern> setting_patterns =
        key.CreateContentSettingsPatterns();
    patterns.emplace_back(setting_patterns.first, setting_patterns.second,
                          base::Value(setting.value()),
                          content_settings::ProviderType::kNone,
                          /*incognito=*/false);
  }

  std::ranges::sort(patterns, std::ranges::greater{},
                    [](const ContentSettingPatternSource& p) {
                      return std::make_pair(p.primary_pattern,
                                            p.secondary_pattern);
                    });

  return patterns;
}

void PermissionOverrides::GrantPermissions(
    base::optional_ref<const url::Origin> requesting_origin,
    base::optional_ref<const url::Origin> embedding_origin,
    const std::vector<blink::PermissionType>& permissions) {
  for (auto type : blink::GetAllPermissionTypes()) {
    Set(requesting_origin, embedding_origin, type,
        base::Contains(permissions, type) ? PermissionStatus::GRANTED
                                          : PermissionStatus::DENIED);
  }
}

std::pair<ContentSettingsPattern, ContentSettingsPattern>
PermissionOverrides::PermissionKey::CreateContentSettingsPatterns() const {
  return std::visit(
      absl::Overload(
          [](const GlobalKey& global_key) {
            return std::make_pair(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard());
          },
          [](const url::Origin& origin) {
            return std::make_pair(
                ContentSettingsPattern::Wildcard(),
                ContentSettingsPattern::FromURLNoWildcard(origin.GetURL()));
          },
          [](const std::pair<net::SchemefulSite, net::SchemefulSite>& key) {
            return std::make_pair(
                ContentSettingsPattern::FromURLToSchemefulSitePattern(
                    key.first.GetURL()),
                ContentSettingsPattern::FromURLToSchemefulSitePattern(
                    key.second.GetURL()));
          },
          [](const std::pair<url::Origin, net::SchemefulSite>& key) {
            return std::make_pair(
                ContentSettingsPattern::FromURLNoWildcard(key.first.GetURL()),
                ContentSettingsPattern::FromURLToSchemefulSitePattern(
                    key.second.GetURL()));
          }),
      scope_);
}

}  // namespace content
