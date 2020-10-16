// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/frame_permission_controller.h"

#include "base/check_op.h"
#include "url/origin.h"

using PermissionStatus = blink::mojom::PermissionStatus;
using PermissionType = content::PermissionType;

namespace {

size_t GetPermissionIndex(PermissionType type) {
  size_t index = static_cast<size_t>(type);
  DCHECK_LT(index, static_cast<size_t>(PermissionType::NUM));
  return index;
}

constexpr PermissionStatus kDefaultPerOriginStatus = PermissionStatus::ASK;

}  // namespace

FramePermissionController::PermissionSet::PermissionSet(
    PermissionStatus initial_state) {
  for (auto& permission : permission_states) {
    permission = initial_state;
  }
}

FramePermissionController::PermissionSet::PermissionSet(
    const PermissionSet& other) = default;

FramePermissionController::PermissionSet&
FramePermissionController::PermissionSet::operator=(
    const PermissionSet& other) = default;

FramePermissionController::FramePermissionController() = default;
FramePermissionController::~FramePermissionController() = default;

void FramePermissionController::SetPermissionState(PermissionType permission,
                                                   const url::Origin& origin,
                                                   PermissionStatus state) {
  auto it = per_origin_permissions_.find(origin);
  if (it == per_origin_permissions_.end()) {
    // Don't create a PermissionSet for |origin| if |state| is set to the
    // per-origin default, since that would have no effect.
    if (state == kDefaultPerOriginStatus)
      return;

    it = per_origin_permissions_
             .insert(
                 std::make_pair(origin, PermissionSet(kDefaultPerOriginStatus)))
             .first;
  }

  it->second.permission_states[GetPermissionIndex(permission)] = state;
}

void FramePermissionController::SetDefaultPermissionState(
    PermissionType permission,
    PermissionStatus state) {
  DCHECK(state != PermissionStatus::ASK);
  default_permissions_.permission_states[GetPermissionIndex(permission)] =
      state;
}

PermissionStatus FramePermissionController::GetPermissionState(
    PermissionType permission,
    const url::Origin& origin) {
  PermissionSet effective = GetEffectivePermissionsForOrigin(origin);
  return effective.permission_states[GetPermissionIndex(permission)];
}

void FramePermissionController::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    const url::Origin& origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  std::vector<PermissionStatus> result;
  result.reserve(permissions.size());

  PermissionSet effective = GetEffectivePermissionsForOrigin(origin);
  for (auto& permission : permissions) {
    result.push_back(
        effective.permission_states[GetPermissionIndex(permission)]);
  }

  std::move(callback).Run(result);
}

FramePermissionController::PermissionSet
FramePermissionController::GetEffectivePermissionsForOrigin(
    const url::Origin& origin) {
  PermissionSet result = default_permissions_;
  auto it = per_origin_permissions_.find(origin);
  if (it != per_origin_permissions_.end()) {
    // Apply per-origin GRANTED and DENIED states. Permissions with the ASK
    // state defer to the defaults.
    for (size_t i = 0; i < it->second.permission_states.size(); ++i) {
      if (it->second.permission_states[i] != kDefaultPerOriginStatus)
        result.permission_states[i] = it->second.permission_states[i];
    }
  }
  return result;
}
