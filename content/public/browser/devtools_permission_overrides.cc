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

void DevToolsPermissionOverrides::Set(const url::Origin& origin,
                                      const PermissionType& permission,
                                      const PermissionStatus status) {
  PermissionOverrides& origin_overrides = overrides_[origin];
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

base::Optional<PermissionStatus> DevToolsPermissionOverrides::Get(
    const url::Origin& origin,
    const PermissionType& type) const {
  auto current_override = overrides_.find(origin);
  if (current_override == overrides_.end())
    return base::nullopt;

  auto new_status = current_override->second.find(type);
  if (new_status == current_override->second.end())
    return base::nullopt;

  return base::make_optional(new_status->second);
}

const PermissionOverrides& DevToolsPermissionOverrides::GetAll(
    const url::Origin& origin) const {
  static const base::NoDestructor<PermissionOverrides> empty_overrides;
  auto it = overrides_.find(origin);
  if (it == overrides_.end())
    return *empty_overrides;
  return it->second;
}

void DevToolsPermissionOverrides::GrantPermissions(
    const url::Origin& origin,
    const std::vector<PermissionType>& permissions) {
  const std::vector<PermissionType>& kAllPermissionTypes =
      GetAllPermissionTypes();
  PermissionOverrides granted_overrides;
  for (const auto& permission : kAllPermissionTypes)
    granted_overrides[permission] = PermissionStatus::DENIED;
  for (const auto& permission : permissions)
    granted_overrides[permission] = PermissionStatus::GRANTED;
  Reset(origin);
  SetAll(origin, granted_overrides);
}

void DevToolsPermissionOverrides::SetAll(const url::Origin& origin,
                                         const PermissionOverrides& overrides) {
  PermissionOverrides& current_override = overrides_[origin];
  for (const auto& setting : overrides)
    current_override[setting.first] = setting.second;
}

}  // namespace content
