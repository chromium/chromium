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

PermissionOverrides::PermissionKey::PermissionKey(
    base::optional_ref<const url::Origin> origin,
    blink::PermissionType type)
    : scope_(origin.has_value() ? std::variant<GlobalKey, url::Origin>(
                                      std::in_place_type<url::Origin>,
                                      origin.value())
                                : std::variant<GlobalKey, url::Origin>(
                                      std::in_place_type<GlobalKey>)),
      type_(type) {}

PermissionOverrides::PermissionKey::PermissionKey(blink::PermissionType type)
    : PermissionKey(std::nullopt, type) {}

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

void PermissionOverrides::Set(base::optional_ref<const url::Origin> origin,
                              blink::PermissionType permission,
                              const blink::mojom::PermissionStatus& status) {
  overrides_[PermissionKey(origin, permission)] = status;

  // Special override status - MIDI_SYSEX is stronger than MIDI, meaning that
  // granting MIDI_SYSEX implies granting MIDI, while denying MIDI implies
  // denying MIDI_SYSEX.
  if (permission == blink::PermissionType::MIDI &&
      status != PermissionStatus::GRANTED) {
    overrides_[PermissionKey(origin, blink::PermissionType::MIDI_SYSEX)] =
        status;
  } else if (permission == blink::PermissionType::MIDI_SYSEX &&
             status == PermissionStatus::GRANTED) {
    overrides_[PermissionKey(origin, blink::PermissionType::MIDI)] = status;
  }
}

std::optional<PermissionStatus> PermissionOverrides::Get(
    const url::Origin& origin,
    blink::PermissionType permission) const {
  const auto* status =
      base::FindOrNull(overrides_, PermissionKey(origin, permission));
  if (!status) {
    status = base::FindOrNull(overrides_, PermissionKey(permission));
  }

  return base::OptionalFromPtr(status);
}

void PermissionOverrides::GrantPermissions(
    base::optional_ref<const url::Origin> origin,
    const std::vector<blink::PermissionType>& permissions) {
  for (auto type : blink::GetAllPermissionTypes()) {
    Set(origin, type,
        base::Contains(permissions, type) ? PermissionStatus::GRANTED
                                          : PermissionStatus::DENIED);
  }
}

}  // namespace content
