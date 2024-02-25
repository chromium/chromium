// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_overrides.h"

#include "base/no_destructor.h"
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

void PermissionOverrides::Set(const std::optional<url::Origin>& origin,
                              blink::PermissionType permission,
                              const blink::mojom::PermissionStatus& status) {
  PermissionOverridesMap& origin_overrides =
      overrides_[origin.value_or(global_overrides_origin_)];
  origin_overrides[permission] = status;

  // Special override status - MIDI_SYSEX is stronger than MIDI, meaning that
  // granting MIDI_SYSEX implies granting MIDI, while denying MIDI implies
  // denying MIDI_SYSEX.
  if (permission == blink::PermissionType::MIDI &&
      status != PermissionStatus::GRANTED) {
    origin_overrides[blink::PermissionType::MIDI_SYSEX] = status;
  } else if (permission == blink::PermissionType::MIDI_SYSEX &&
             status == PermissionStatus::GRANTED) {
    origin_overrides[blink::PermissionType::MIDI] = status;
  }
}

std::optional<PermissionStatus> PermissionOverrides::Get(
    const url::Origin& origin,
    blink::PermissionType permission) const {
  auto current_override = overrides_.find(origin);
  if (current_override == overrides_.end())
    current_override = overrides_.find(global_overrides_origin_);
  if (current_override == overrides_.end())
    return std::nullopt;

  auto new_status = current_override->second.find(permission);
  if (new_status != current_override->second.end())
    return std::make_optional(new_status->second);
  return std::nullopt;
}

const PermissionOverridesMap& PermissionOverrides::GetAllForTest(
    const std::optional<url::Origin>& origin) const {
  static const base::NoDestructor<PermissionOverridesMap> empty_overrides;
  auto it = origin ? overrides_.find(*origin) : overrides_.end();
  if (it == overrides_.end())
    it = overrides_.find(global_overrides_origin_);
  if (it == overrides_.end())
    return *empty_overrides;
  return it->second;
}

void PermissionOverrides::Reset(const std::optional<url::Origin>& origin) {
  overrides_.erase(origin.value_or(global_overrides_origin_));
}

void PermissionOverrides::GrantPermissions(
    const std::optional<url::Origin>& origin,
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
