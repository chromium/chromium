// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/devtools_permission_overrides.h"

#include "base/no_destructor.h"

namespace content {
using PermissionOverrides = DevToolsPermissionOverrides::PermissionOverrides;
using PermissionStatus = blink::mojom::PermissionStatus;

DevToolsPermissionOverrides::DevToolsPermissionOverrides() = default;
DevToolsPermissionOverrides::~DevToolsPermissionOverrides() = default;
DevToolsPermissionOverrides::DevToolsPermissionOverrides(
    DevToolsPermissionOverrides&& other) = default;
DevToolsPermissionOverrides& DevToolsPermissionOverrides::operator=(
    DevToolsPermissionOverrides&& other) = default;

void DevToolsPermissionOverrides::Set(
    const absl::optional<url::Origin>& origin,
    PermissionType permission,
    const blink::mojom::PermissionStatus& status) {
  PermissionOverrides& origin_overrides =
      overrides_[origin.value_or(global_overrides_origin_)];
  origin_overrides[permission] = status;

  // Special override status - MIDI_SYSEX is stronger than MIDI, meaning that
  // granting MIDI_SYSEX implies granting MIDI, while denying MIDI implies
  // denying MIDI_SYSEX.
  if (permission == PermissionType::MIDI &&
      status != PermissionStatus::GRANTED) {
    origin_overrides[PermissionType::MIDI_SYSEX] = status;
  } else if (permission == PermissionType::MIDI_SYSEX &&
             status == PermissionStatus::GRANTED) {
    origin_overrides[PermissionType::MIDI] = status;
  }
}

absl::optional<PermissionStatus> DevToolsPermissionOverrides::Get(
    const url::Origin& origin,
    PermissionType permission) const {
  auto current_override = overrides_.find(origin);
  if (current_override == overrides_.end())
    current_override = overrides_.find(global_overrides_origin_);
  if (current_override == overrides_.end())
    return absl::nullopt;

  auto new_status = current_override->second.find(permission);
  if (new_status != current_override->second.end())
    return absl::make_optional(new_status->second);
  return absl::nullopt;
}

const PermissionOverrides& DevToolsPermissionOverrides::GetAll(
    const absl::optional<url::Origin>& origin) const {
  static const base::NoDestructor<PermissionOverrides> empty_overrides;
  auto it = origin ? overrides_.find(*origin) : overrides_.end();
  if (it == overrides_.end())
    it = overrides_.find(global_overrides_origin_);
  if (it == overrides_.end())
    return *empty_overrides;
  return it->second;
}

void DevToolsPermissionOverrides::Reset(
    const absl::optional<url::Origin>& origin) {
  overrides_.erase(origin.value_or(global_overrides_origin_));
}

void DevToolsPermissionOverrides::GrantPermissions(
    const absl::optional<url::Origin>& origin,
    const std::vector<PermissionType>& permissions) {
  const std::vector<PermissionType>& kAllPermissionTypes =
      GetAllPermissionTypes();
  PermissionOverrides granted_overrides;
  for (const auto& permission : kAllPermissionTypes)
    granted_overrides[permission] = PermissionStatus::DENIED;
  for (const auto& permission : permissions)
    granted_overrides[permission] = PermissionStatus::GRANTED;
  Reset(origin);
  for (const auto& setting : granted_overrides)
    Set(origin, setting.first, setting.second);
}

}  // namespace content
