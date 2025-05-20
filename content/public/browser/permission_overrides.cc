// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_overrides.h"

#include "base/types/optional_ref.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {
using PermissionOverridesMap =
    base::flat_map<blink::PermissionType, blink::mojom::PermissionStatus>;
using PermissionStatus = blink::mojom::PermissionStatus;

PermissionOverrides::PermissionOverrides() = default;
PermissionOverrides::~PermissionOverrides() = default;
PermissionOverrides::PermissionOverrides(PermissionOverrides&& other) = default;
PermissionOverrides& PermissionOverrides::operator=(
    PermissionOverrides&& other) = default;

void PermissionOverrides::Set(base::optional_ref<const url::Origin> origin,
                              blink::PermissionType permission,
                              const blink::mojom::PermissionStatus& status) {
  const url::Origin& key_origin =
      origin.has_value() ? *origin : global_overrides_origin_;
  overrides_[{key_origin, permission}] = status;

  // Special override status - MIDI_SYSEX is stronger than MIDI, meaning that
  // granting MIDI_SYSEX implies granting MIDI, while denying MIDI implies
  // denying MIDI_SYSEX.
  if (permission == blink::PermissionType::MIDI &&
      status != PermissionStatus::GRANTED) {
    overrides_[{key_origin, blink::PermissionType::MIDI_SYSEX}] = status;
  } else if (permission == blink::PermissionType::MIDI_SYSEX &&
             status == PermissionStatus::GRANTED) {
    overrides_[{key_origin, blink::PermissionType::MIDI}] = status;
  }
}

std::optional<PermissionStatus> PermissionOverrides::Get(
    const url::Origin& origin,
    blink::PermissionType permission) const {
  auto current_override = overrides_.find({origin, permission});
  if (current_override == overrides_.end())
    current_override = overrides_.find({global_overrides_origin_, permission});
  if (current_override == overrides_.end())
    return std::nullopt;

  return current_override->second;
}

PermissionOverridesMap PermissionOverrides::GetAllForTest(
    base::optional_ref<const url::Origin> origin) const {
  PermissionOverridesMap output;
  if (origin) {
    for (const auto& [key, status] : overrides_) {
      if (key.first == origin) {
        output[key.second] = status;
      }
    }
  }
  for (const auto& [key, status] : overrides_) {
    if (key.first == global_overrides_origin_ && !output.contains(key.second)) {
      output[key.second] = status;
    }
  }

  return output;
}

void PermissionOverrides::Reset(base::optional_ref<const url::Origin> origin) {
  const url::Origin& key_origin =
      origin.has_value() ? *origin : global_overrides_origin_;
  base::EraseIf(overrides_, [&](const auto& pair) {
    const auto& [key, status] = pair;
    return key.first == key_origin;
  });
}

void PermissionOverrides::GrantPermissions(
    base::optional_ref<const url::Origin> origin,
    const std::vector<blink::PermissionType>& permissions) {
  const std::vector<blink::PermissionType>& kAllPermissionTypes =
      blink::GetAllPermissionTypes();
  PermissionOverridesMap granted_overrides;
  for (const auto& permission : kAllPermissionTypes)
    granted_overrides[permission] = PermissionStatus::DENIED;
  for (const auto& permission : permissions)
    granted_overrides[permission] = PermissionStatus::GRANTED;
  Reset(origin);
  for (const auto& setting : granted_overrides)
    Set(origin, setting.first, setting.second);
}

}  // namespace content
