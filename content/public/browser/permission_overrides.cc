// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_overrides.h"

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/types/optional_ref.h"
#include "base/types/optional_util.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {
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
  const auto* status = base::FindOrNull(overrides_, {origin, permission});
  if (!status) {
    status =
        base::FindOrNull(overrides_, {global_overrides_origin_, permission});
  }

  return base::OptionalFromPtr(status);
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
  const auto granted_overrides =
      base::MakeFlatMap<blink::PermissionType, PermissionStatus>(
          blink::GetAllPermissionTypes(), {}, [&](blink::PermissionType type) {
            return std::make_pair(type, base::Contains(permissions, type)
                                            ? PermissionStatus::GRANTED
                                            : PermissionStatus::DENIED);
          });
  Reset(origin);
  for (const auto& setting : granted_overrides)
    Set(origin, setting.first, setting.second);
}

}  // namespace content
